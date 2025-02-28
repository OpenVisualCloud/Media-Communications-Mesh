/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_buf.h"
#include "mesh_conn.h"
#include "mesh_logger.h"

namespace mesh {

BufferContext::BufferContext(ConnectionContext *conn)
{
    *(MeshConnection **)&__public.conn = (MeshConnection *)conn;
}

int BufferContext::dequeue(int timeout_ms)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    int err;
    buf = mesh_internal_ops.dequeue_buf(conn->handle, timeout_ms, &err);
    if (!buf)
        return err ? err : -MESH_ERR_CONN_CLOSED;

    if (buf->len != conn->cfg_json.buf_parts.total_size()) {
        mesh_internal_ops.enqueue_buf(conn->handle, buf);
        return -MESH_ERR_BAD_BUF_LEN;
    }

    auto base_ptr = (char *)buf->data;
    auto sysdata = (BufferSysData *)(base_ptr + conn->cfg_json.buf_parts.sysdata.offset);
    auto payload_ptr = (void *)(base_ptr + conn->cfg_json.buf_parts.payload.offset);
    auto metadata_ptr = (void *)(base_ptr + conn->cfg_json.buf_parts.metadata.offset);

    if (conn->cfg_json.kind == MESH_CONN_KIND_SENDER) {
        sysdata->payload_len = conn->cfg_json.calculated_payload_size;
        sysdata->metadata_len = 0;
    } else {
        if (sysdata->payload_len > conn->cfg_json.buf_parts.payload.size)
            sysdata->payload_len = conn->cfg_json.buf_parts.payload.size;
        if (sysdata->metadata_len > conn->cfg_json.buf_parts.metadata.size)
            sysdata->metadata_len = conn->cfg_json.buf_parts.metadata.size;
    }

    *(void **)&__public.payload_ptr = payload_ptr;
    *(size_t *)&__public.payload_len = sysdata->payload_len;
    *(void **)&__public.metadata_ptr = metadata_ptr;
    *(size_t *)&__public.metadata_len = sysdata->metadata_len;

    return 0;
}

int BufferContext::enqueue(int timeout_ms)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (conn->cfg_json.kind == MESH_CONN_KIND_SENDER) {
        auto base_ptr = (char *)buf->data;
        auto sysdata = (BufferSysData *)(base_ptr + conn->cfg_json.buf_parts.sysdata.offset);

        sysdata->payload_len = __public.payload_len;
        sysdata->metadata_len = __public.metadata_len;
        sysdata->seq = 0;          // TODO: Implement incremental seq numbers
        sysdata->timestamp_ms = 0; // TODO: Implement timestamping
    }

    /**
     * TODO: Add timeout handling
     */

    return mesh_internal_ops.enqueue_buf(conn->handle, buf);
}

int BufferContext::setPayloadLen(size_t size)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (size > conn->cfg_json.buf_parts.payload.size)
        return -MESH_ERR_BAD_BUF_LEN;

    *(size_t *)&__public.payload_len = size;
    return 0;
}

int BufferContext::setMetadataLen(size_t size)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (size > conn->cfg_json.buf_parts.metadata.size)
        return -MESH_ERR_BAD_BUF_LEN;

    *(size_t *)&__public.metadata_len = size;
    return 0;
}

size_t BufferPartitions::total_size() const {
    return payload.size + metadata.size + sysdata.size;
}

} // namespace mesh
