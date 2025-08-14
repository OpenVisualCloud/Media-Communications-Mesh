/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_client.h"
#include "mesh_conn.h"
#include "mesh_buf.h"
#include "mesh_dp_legacy.h"
#include "mesh_logger.h"

using namespace mesh;

/**
 * Create a new mesh client
 */
int mesh_create_client(MeshClient **mc, const char *cfg)
{
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if (!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    ClientContext *mc_ctx = new(std::nothrow) ClientContext();
    if (!mc_ctx) {
        *mc = NULL;
        return -ENOMEM;
    }

    int ret = mc_ctx->init(cfg);
    if (ret) {
        delete mc_ctx;
        return ret;
    }

    *mc = (MeshClient *)mc_ctx;

    return 0;
}

/**
 * Delete mesh client
 */
int mesh_delete_client(MeshClient **mc)
{
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    ClientContext *mc_ctx = (ClientContext *)(*mc);

    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    int err = mc_ctx->shutdown();
    if (err)
        return err;

    delete mc_ctx;
    *mc = NULL;

    return 0;
}

int mesh_create_tx_connection(MeshClient *mc, MeshConnection **conn, const char *cfg)
{
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if (!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    ClientContext *mc_ctx = (ClientContext *)mc;

    auto err = mc_ctx->create_connection(conn, MESH_CONN_KIND_SENDER, cfg);
    if (err)
        return err;

    ConnectionContext *conn_ctx = (ConnectionContext *)(*conn);

    return conn_ctx->establish();
}

int mesh_create_rx_connection(MeshClient *mc, MeshConnection **conn, const char *cfg)
{
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if (!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    ClientContext *mc_ctx = (ClientContext *)mc;

    auto err = mc_ctx->create_connection(conn, MESH_CONN_KIND_RECEIVER, cfg);
    if (err)
        return err;

    ConnectionContext *conn_ctx = (ConnectionContext *)(*conn);

    return conn_ctx->establish();
}

/**
 * Shutdown a mesh connection
 */
int mesh_shutdown_connection(MeshConnection *conn)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    return conn_ctx->shutdown();
}

/**
 * Delete mesh connection
 */
int mesh_delete_connection(MeshConnection **conn)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)(*conn);

    if (!conn_ctx)
        return -MESH_ERR_BAD_CONN_PTR;

    int err = conn_ctx->shutdown();
    if (err)
        return err;

    if (!conn_ctx->__public.client)
        return -MESH_ERR_BAD_CLIENT_PTR;

    delete conn_ctx;
    *conn = NULL;

    return 0;
}

/**
 * Get buffer from mesh connection
 */
int mesh_get_buffer(MeshConnection *conn, MeshBuffer **buf)
{
    return mesh_get_buffer_timeout(conn, buf, MESH_TIMEOUT_DEFAULT);
}

/**
 * Get buffer from mesh connection with timeout
 */
int mesh_get_buffer_timeout(MeshConnection *conn, MeshBuffer **buf,
                            int timeout_ms)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    return conn_ctx->get_buffer(buf, timeout_ms);
}

/**
 * Put buffer to mesh connection
 */
int mesh_put_buffer(MeshBuffer **buf)
{
    return mesh_put_buffer_timeout(buf, MESH_TIMEOUT_DEFAULT);
}

/**
 * Put buffer to mesh connection with timeout
 */
int mesh_put_buffer_timeout(MeshBuffer **buf, int timeout_ms)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    BufferContext *buf_ctx = (BufferContext *)(*buf);

    if (!buf_ctx)
        return -MESH_ERR_BAD_BUF_PTR;

    int err = buf_ctx->put(timeout_ms);

    delete buf_ctx;
    *buf = NULL;

    return err;
}

int mesh_buffer_set_payload_len(MeshBuffer *buf, size_t len)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    BufferContext *buf_ctx = (BufferContext *)buf;

    return buf_ctx->setPayloadLen(len);
}

int mesh_buffer_set_metadata_len(MeshBuffer *buf, size_t len)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    BufferContext *buf_ctx = (BufferContext *)buf;

    return buf_ctx->setMetadataLen(len);
}

/**
 * Get text description of an error code.
 */
const char * mesh_err2str(int err)
{
    switch (err) {
    case -MESH_ERR_BAD_CLIENT_PTR:
        return "Bad client pointer";
    case -MESH_ERR_BAD_CONN_PTR:
        return "Bad connection pointer";
    case -MESH_ERR_BAD_CONFIG_PTR:
        return "Bad configuration pointer";
    case -MESH_ERR_BAD_BUF_PTR:
        return "Bad buffer pointer";
    case -MESH_ERR_BAD_BUF_LEN:
        return "Bad buffer length";
    case -MESH_ERR_CLIENT_FAILED:
        return "Client creation failed";
    case -MESH_ERR_CLIENT_CONFIG_INVAL:
        return "Invalid parameters in client configuration";
    case -MESH_ERR_MAX_CONN:
        return "Reached max number of connections";
    case -MESH_ERR_FOUND_ALLOCATED:
        return "Found allocated resources";
    case -MESH_ERR_CONN_FAILED:
        return "Connection creation failed";
    case -MESH_ERR_CONN_CONFIG_INVAL:
        return "Invalid parameters in connection configuration";
    case -MESH_ERR_CONN_CONFIG_INCOMPAT:
        return "Incompatible parameters in connection configuration";
    case -MESH_ERR_CONN_CLOSED:
        return "Connection is closed";
    case -MESH_ERR_TIMEOUT:
        return "Timeout occurred";
    case -MESH_ERR_NOT_IMPLEMENTED:
        return "Feature not implemented yet";
    default:
        return "Unknown error code";
    }
}
