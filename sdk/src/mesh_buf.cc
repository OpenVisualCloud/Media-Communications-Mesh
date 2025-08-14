/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_buf.h"
#include "mesh_conn.h"

namespace mesh {

BufferContext::BufferContext(ConnectionContext *conn)
{
    *(MeshConnection **)&__public.conn = (MeshConnection *)conn;
}

int BufferContext::put(int timeout_ms)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    return conn->put_buffer((MeshBuffer *)this, timeout_ms);
}

int BufferContext::setPayloadLen(size_t size)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (size > conn->cfg.buf_parts.payload.size)
        return -MESH_ERR_BAD_BUF_LEN;

    *(size_t *)&__public.payload_len = size;
    return 0;
}

int BufferContext::setMetadataLen(size_t size)
{
    ConnectionContext *conn = (ConnectionContext *)__public.conn;
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (size > conn->cfg.buf_parts.metadata.size)
        return -MESH_ERR_BAD_BUF_LEN;

    *(size_t *)&__public.metadata_len = size;
    return 0;
}

size_t BufferPartitions::total_size() const {
    return payload.size + metadata.size + sysdata.size;
}

} // namespace mesh
