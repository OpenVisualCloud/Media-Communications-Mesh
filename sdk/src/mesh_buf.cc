/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_buf.h"

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

    *(void **)&__public.payload_ptr = buf->data;
    *(size_t *)&__public.payload_len = buf->len;
    *(void **)&__public.metadata_ptr = nullptr;
    *(size_t *)&__public.metadata_len = 0;
    return 0;
}

int BufferContext::enqueue(int timeout_ms)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

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

    buf->len = size;
    *(size_t *)&__public.payload_len = buf->len;
    return 0;
}

int BufferContext::setMetadataLen(size_t size)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    //TODO: Add metadata len handling
    //buf->metadata_len = size;
    *(size_t *)&__public.metadata_len = size;
    return -MESH_ERR_NOT_IMPLEMENTED;
}

} // namespace mesh
