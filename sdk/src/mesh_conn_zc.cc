/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_conn_zc.h"
#include <string.h>
#include <bsd/string.h>
#include <unistd.h>
#include "mesh_buf.h"
#include "mesh_sdk_api.h"
#include "mesh_logger.h"
#include "json.hpp"
#include <thread>
#include <stop_token>
#include "mesh_dp_legacy.h"

namespace mesh {

int ZeroCopyConnectionContext::establish()
{
    ClientContext *mc_ctx = (ClientContext *)__public.client;
    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    auto conn = mesh_internal_ops.create_conn_zero_copy(mc_ctx->proxy_client, cfg,
                                                        temporary_id);
    if (!conn)
        return -MESH_ERR_CONN_FAILED;

    {
        std::lock_guard<std::mutex> lk(mc_ctx->mx);
        proxy_conn = conn;
    }

    auto err = mesh_internal_ops.configure_conn_zero_copy(this);
    if (!err)
        return err;

    return 0;
}

int ZeroCopyConnectionContext::shutdown()
{
    ClientContext *mc_ctx = (ClientContext *)__public.client;
    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if (proxy_conn) {
        /** In Sender mode, delay for 50 ms to allow for completing
         * transmission of all buffers sitting in the memif queue
         * before destroying the connection.
         *
         * TODO: Replace the delay with polling of the actual memif
         * queue status.
         */
        if (cfg.kind == MESH_CONN_KIND_SENDER)
            usleep(50000);

        mesh_internal_ops.destroy_conn_zero_copy(this);

        proxy_conn = NULL;
        handle = NULL;
    }

    return 0;
}

static char tempbuf[1024*1024];

int ZeroCopyConnectionContext::get_buffer(MeshBuffer **buf, int timeout_ms)
{
    char *base_ptr;
    uint32_t sz = 0;

    if (cfg.kind == MESH_CONN_KIND_RECEIVER) {
        auto interval = timeout_ms == MESH_TIMEOUT_INFINITE ? 0 : timeout_ms;
        auto tctx = context::WithTimeout(gctx, std::chrono::milliseconds(interval));

        auto rx_evt = zero_copy_rx_ch.receive(tctx);
        if (!rx_evt.has_value()) {
            return -MESH_ERR_CONN_CLOSED;
        }
        auto evt = rx_evt.value();

        base_ptr = (char *)evt.ptr;
        sz = evt.sz;
    } else {
        base_ptr = tempbuf;
    }

    auto sysdata = (BufferSysData *)(base_ptr + cfg.buf_parts.sysdata.offset);
    auto payload_ptr = (void *)(base_ptr + cfg.buf_parts.payload.offset);
    auto metadata_ptr = (void *)(base_ptr + cfg.buf_parts.metadata.offset);

    // DEBUG
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sysdata->payload_len = cfg.calculated_payload_size;
    // DEBUG

    if (cfg.kind == MESH_CONN_KIND_SENDER) {
        sysdata->payload_len = cfg.calculated_payload_size;
        sysdata->metadata_len = 0;
    } else {
        if (sysdata->payload_len > cfg.buf_parts.payload.size)
            sysdata->payload_len = cfg.buf_parts.payload.size;
        if (sysdata->metadata_len > cfg.buf_parts.metadata.size)
            sysdata->metadata_len = cfg.buf_parts.metadata.size;

        metrics.inbound_bytes += sz;
    }

    auto buf_ctx = new(std::nothrow) BufferContext(this);
    if (!buf_ctx) {
        metrics.transactions_failed++;
        metrics.errors++;
        return -ENOMEM;
    }

    *(void **)&buf_ctx->__public.payload_ptr = payload_ptr;
    *(size_t *)&buf_ctx->__public.payload_len = sysdata->payload_len;
    *(void **)&buf_ctx->__public.metadata_ptr = metadata_ptr;
    *(size_t *)&buf_ctx->__public.metadata_len = sysdata->metadata_len;

    *buf = (MeshBuffer *)buf_ctx;

    if (cfg.kind == MESH_CONN_KIND_RECEIVER) {
        metrics.outbound_bytes += sz;
        metrics.transactions_succeeded++;
    }

    // log::debug("HERE %u", sysdata->payload_len);

    return 0;
}

int ZeroCopyConnectionContext::put_buffer(MeshBuffer *buf, int timeout_ms)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    BufferContext *buf_ctx = (BufferContext *)buf;

    if (ctx.cancelled())
        return -MESH_ERR_CONN_CLOSED;

    if (cfg.kind == MESH_CONN_KIND_SENDER) {
        auto base_ptr = tempbuf;
        auto sysdata = (BufferSysData *)(base_ptr + cfg.buf_parts.sysdata.offset);

        sysdata->payload_len = buf_ctx->__public.payload_len;
        sysdata->metadata_len = buf_ctx->__public.metadata_len;
        sysdata->seq = 0;          // TODO: Implement incremental seq numbers
        sysdata->timestamp_ms = 0; // TODO: Implement timestamping

        auto sz = cfg.buf_parts.total_size();
        metrics.inbound_bytes += sz;

        uint32_t sent = 0;
        auto res = gw_rx.transmit(ctx, base_ptr, sz, sent);

        metrics.outbound_bytes += sent;

        if (res == zerocopy::gateway::Result::success)
            metrics.transactions_succeeded++;
        else {
            metrics.transactions_failed++;
            metrics.errors++;
        }

        if (res != zerocopy::gateway::Result::success)
            return -1;
    }

    return 0;
}

} // namespace mesh
