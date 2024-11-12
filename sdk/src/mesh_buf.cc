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

    *(void **)&__public.data = buf->data;
    *(size_t *)&__public.data_len = buf->len;
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

} // namespace mesh
