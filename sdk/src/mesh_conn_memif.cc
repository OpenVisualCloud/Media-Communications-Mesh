/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_conn_memif.h"
#include <string.h>
#include <bsd/string.h>
#include <unistd.h>
#include "mesh_buf_memif.h"
#include "mesh_sdk_api.h"
#include "mesh_logger.h"
#include "json.hpp"
#include <thread>
#include <stop_token>
#include "mesh_dp_legacy.h"

namespace mesh {

int MemifConnectionContext::establish()
{
    int err;

    if (handle)
        return -MESH_ERR_BAD_CONN_PTR;

    ClientContext *mc_ctx = (ClientContext *)__public.client;
    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    proxy_conn = mesh_internal_ops.create_conn(mc_ctx->proxy_client, cfg);
    if (!proxy_conn) {
        handle = NULL;
        return -MESH_ERR_CONN_FAILED;
    }
    handle = *(mcm_conn_context **)proxy_conn; // unsafe type casting
    if (!handle)
        return -MESH_ERR_CONN_FAILED;

    if (cfg.buf_parts.total_size() != handle->frame_size)
        return -MESH_ERR_CONN_FAILED;

    return 0;
}

int MemifConnectionContext::shutdown()
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
        if (cfg.kind == MESH_CONN_KIND_SENDER) {
            usleep(50000);
            mesh_internal_ops.destroy_conn(proxy_conn);
        } else {
            /**
             * In Receiver mode, start a thread to drain the queue continuously
             * while the DeleteConnection request is being processed in Media
             * Proxy and Mesh Agent. This prevents the following errors at
             * connection shutdown:
             *     Failed to alloc memif buffer: Ring buffer full.
             */
            std::stop_source ss;

            std::jthread th([&] {
                while (!ss.stop_requested()) {
                    MeshBuffer *buf = nullptr;

                    auto err = get_buffer(&buf, 500);
                    if (err)
                        break;

                    err = put_buffer(buf, 500);
                    if (err)
                        break;
                }
            });

            mesh_internal_ops.destroy_conn(proxy_conn);

            ss.request_stop();
        }

        proxy_conn = NULL;
        handle = NULL;
    }

    return 0;
}

int MemifConnectionContext::get_buffer(MeshBuffer **buf, int timeout_ms)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    if (timeout_ms == MESH_TIMEOUT_DEFAULT && __public.client)
        timeout_ms = ((ClientContext *)__public.client)->cfg.default_timeout_us;

    if (ctx.cancelled())
        return -MESH_ERR_CONN_CLOSED;

    int err;
    auto memif_buf = mesh_internal_ops.dequeue_buf(handle, timeout_ms, &err);
    if (!memif_buf) {
        if (!err ||
            err == MEMIF_ERR_POLL_CANCEL ||
            err == MEMIF_ERR_DISCONNECT ||
            err == MEMIF_ERR_DISCONNECTED)
            return -MESH_ERR_CONN_CLOSED;
        else
            return err;
    }

    if (memif_buf->len != cfg.buf_parts.total_size()) {
        mesh_internal_ops.enqueue_buf(handle, memif_buf);
        return -MESH_ERR_BAD_BUF_LEN;
    }

    auto base_ptr = (char *)memif_buf->data;
    auto sysdata = (BufferSysData *)(base_ptr + cfg.buf_parts.sysdata.offset);
    auto payload_ptr = (void *)(base_ptr + cfg.buf_parts.payload.offset);
    auto metadata_ptr = (void *)(base_ptr + cfg.buf_parts.metadata.offset);

    if (cfg.kind == MESH_CONN_KIND_SENDER) {
        sysdata->payload_len = cfg.calculated_payload_size;
        sysdata->metadata_len = 0;
    } else {
        if (sysdata->payload_len > cfg.buf_parts.payload.size)
            sysdata->payload_len = cfg.buf_parts.payload.size;
        if (sysdata->metadata_len > cfg.buf_parts.metadata.size)
            sysdata->metadata_len = cfg.buf_parts.metadata.size;
    }

    auto buf_ctx = new(std::nothrow) MemifBufferContext(this);
    if (!buf_ctx) {
        mesh_internal_ops.enqueue_buf(handle, memif_buf);
        return -ENOMEM;
    }

    buf_ctx->buf = memif_buf;

    *(void **)&buf_ctx->__public.payload_ptr = payload_ptr;
    *(size_t *)&buf_ctx->__public.payload_len = sysdata->payload_len;
    *(void **)&buf_ctx->__public.metadata_ptr = metadata_ptr;
    *(size_t *)&buf_ctx->__public.metadata_len = sysdata->metadata_len;

    *buf = (MeshBuffer *)buf_ctx;

    return 0;
}

int MemifConnectionContext::put_buffer(MeshBuffer *buf, int timeout_ms)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    MemifBufferContext *buf_ctx = (MemifBufferContext *)buf;

    if (ctx.cancelled())
        return -MESH_ERR_CONN_CLOSED;

    if (cfg.kind == MESH_CONN_KIND_SENDER) {
        auto base_ptr = (char *)buf_ctx->buf->data;
        auto sysdata = (BufferSysData *)(base_ptr + cfg.buf_parts.sysdata.offset);

        sysdata->payload_len = buf_ctx->__public.payload_len;
        sysdata->metadata_len = buf_ctx->__public.metadata_len;
        sysdata->seq = 0;          // TODO: Implement incremental seq numbers
        sysdata->timestamp_ms = 0; // TODO: Implement timestamping
    }

    /**
     * TODO: Add timeout handling
     */

    return mesh_internal_ops.enqueue_buf(handle, buf_ctx->buf);
}

} // namespace mesh
