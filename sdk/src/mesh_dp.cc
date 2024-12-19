/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_client.h"
#include "mesh_conn.h"
#include "mesh_buf.h"

using namespace mesh;

/**
 * Create a new mesh client
 */
int mesh_create_client(MeshClient **mc, MeshClientConfig *cfg)
{
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    ClientContext *mc_ctx = new(std::nothrow) ClientContext(cfg);
    if (!mc_ctx) {
        *mc = NULL;
        return -ENOMEM;
    }

    mc_ctx->init();

    *mc = (MeshClient *)mc_ctx;

    return 0;
}

/**
 * Create a new mesh client
 */
int mesh_create_client_json(MeshClient **mc, const char *cfg) {
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if(!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    ClientContext *mc_ctx = new (std::nothrow) ClientContext();
    if (!mc_ctx) {
        *mc = NULL;
        return -ENOMEM;
    }

    std::string cfg_string(cfg);
    int ret = mc_ctx->init_json(cfg_string);
    if(ret){
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

/**
 * Create a new mesh connection
 */
int mesh_create_connection(MeshClient *mc, MeshConnection **conn)
{
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    ClientContext *mc_ctx = (ClientContext *)mc;

    return mc_ctx->create_conn(conn);
}

int mesh_create_tx_connection(MeshClient *mc, MeshConnection **conn, const char *cfg){
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if(!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    ClientContext *mc_ctx = (ClientContext *)mc;

    int ret = mc_ctx->create_conn_json(conn);
    if(ret)
        return ret;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    //TODO: Pass connection configuration to establish connection
    //std::string cfg_json(cfg);
    //return conn_ctx->establish(MESH_CONN_KIND_SENDER/*, cfg_json*/);
    return -MESH_ERR_BAD_CONFIG_PTR;
}

int mesh_create_rx_connection(MeshClient *mc, MeshConnection **conn, const char *cfg){
    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if(!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    ClientContext *mc_ctx = (ClientContext *)mc;

    int ret = mc_ctx->create_conn_json(conn);
    if(ret)
        return ret;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    //TODO: Pass connection configuration to establish connection
    //std::string cfg_json(cfg);
    //return conn_ctx->establish(MESH_CONN_KIND_RECEIVER/*, cfg_json*/);
    return -MESH_ERR_BAD_CONFIG_PTR;
}

/**
 * Apply configuration to setup Single node direct connection via memif
 */
int mesh_apply_connection_config_memif(MeshConnection *conn, MeshConfig_Memif *cfg)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    return conn_ctx->apply_config_memif(cfg);
}

/**
 * Apply configuration to setup SMPTE ST2110-xx connection via Media Proxy
 */
int mesh_apply_connection_config_st2110(MeshConnection *conn, MeshConfig_ST2110 *cfg)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    return conn_ctx->apply_config_st2110(cfg);
}

/**
 * Apply configuration to setup RDMA connection via Media Proxy
 */
int mesh_apply_connection_config_rdma(MeshConnection *conn, MeshConfig_RDMA *cfg)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    return conn_ctx->apply_config_rdma(cfg);
}

/**
 * Apply configuration to setup connection payload for Video frames
 */
int mesh_apply_connection_config_video(MeshConnection *conn, MeshConfig_Video *cfg)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    return conn_ctx->apply_config_video(cfg);
}

/**
 * Apply configuration to setup connection payload for Audio packets
 */
int mesh_apply_connection_config_audio(MeshConnection *conn, MeshConfig_Audio *cfg)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    return conn_ctx->apply_config_audio(cfg);
}

/**
 * Establish a mesh connection
 */
int mesh_establish_connection(MeshConnection *conn, int kind)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    ConnectionContext *conn_ctx = (ConnectionContext *)conn;

    return conn_ctx->establish(kind);
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

    return conn_ctx->get_buffer_timeout(buf, timeout_ms);
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

    int err = buf_ctx->enqueue(timeout_ms);
    if (err)
        return err;

    delete buf_ctx;
    *buf = NULL;

    return 0;
}

int mesh_buffer_set_payload_len(MeshBuffer **buf, size_t len)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    BufferContext *buf_ctx = (BufferContext *)(*buf);

    if (!buf_ctx)
        return -MESH_ERR_BAD_BUF_PTR;

    return buf_ctx->setPayloadLen(len);
}

int mesh_buffer_set_metadata_len(MeshBuffer **buf, size_t len)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    BufferContext *buf_ctx = (BufferContext *)(*buf);

    if (!buf_ctx)
        return -MESH_ERR_BAD_BUF_PTR;

    return buf_ctx->setMetadataLen(len);
}

/**
 * Get text description of an error code.
 */
const char *mesh_err2str(int err)
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
    case -MESH_ERR_INTERNAL_NOT_IMPLEMENTED:
        return "Not implemented yet";
    default:
        return "Unknown error code";
    }
}
