/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <bsd/string.h>
#include <stdlib.h>
#include <unistd.h>

#include "memif_impl.h"
#include "udp_impl.h"
#include "logger.h"
#include "mcm_dp.h"
#include "media_proxy_ctrl.h"

/* Create a new mesh client */
int mesh_create_client(MeshClient *mc, MeshClientConfig *cfg)
{
    mc = calloc(1, siezof(MeshClientConfig));
    if (mc==NULL) return MESH_CANNOT_CREATE_MESH_CLIENT;

    memcpy(mc, cfg, sideof(MeshClientConfig));   
    return 0;
}

/* Delete mesh client */
int mesh_delete_client(MeshClient *mc);
{
    if (mc)
    {
        free(mc);
        return 0;        
    }
    else
        return MESH_ERR_BAD_CLIENT_HANDLE;
}

static void parse_memif_param(mcm_conn_param* request, memif_socket_args_t* memif_socket_args, memif_conn_args_t* memif_conn_args)
{
    char type_str[8] = "";
    char interface_name[32];

    if (request->type == is_tx) {
        strlcpy(type_str, "tx", sizeof(type_str));
    } else {
        strlcpy(type_str, "rx", sizeof(type_str));
    }

    memif_conn_args->is_master = request->memif_interface.is_master;
    memif_conn_args->interface_id = request->memif_interface.interface_id;

    snprintf(memif_socket_args->app_name, sizeof(memif_socket_args->app_name), "memif_%s_%d", type_str, atoi(request->local_addr.port));
    snprintf(interface_name, sizeof(interface_name), "memif_%s_%d", type_str, atoi(request->local_addr.port));
    strlcpy((char*)memif_conn_args->interface_name, interface_name, sizeof(memif_conn_args->interface_name));
    if (!strlen(request->memif_interface.socket_path)) {
        strlcpy(memif_socket_args->path, "/run/mcm/mcm_rx_memif.sock", sizeof(memif_socket_args->path));
    } else {
        strlcpy(memif_socket_args->path, request->memif_interface.socket_path, sizeof(memif_socket_args->path));
    }

    memif_conn_args->buffer_size = request->payload_args.video_args.width * request->payload_args.video_args.height * 4;
    memif_conn_args->log2_ring_size = 4;
}

/* Connect to MCM media proxy daemon through memif. */
int mcm_create_connection_proxy(MeshClient *mc, MeshConnection *conn)
{
    int ret = 0;
    int sockfd = 0;
    uint32_t session_id = 0;
    memif_conn_param memif_param = {};

    /* Get media-proxy address. */
    ret = get_media_proxy_addr(mc->proxy_addr);
    if (ret != 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to get MCM Media-Proxy address.");
        return NULL;
    }

    mesh_log(mc, MESH_LOG_VERBOSE, ("Connecting to MCM Media-Proxy: %s:%s", mc->proxy_addr.ip, mc->proxy_addr.port);

    /* Create socket connection with media-proxy. */
    sockfd = open_socket(mc->proxy_addr);
    if (sockfd < 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to create network connection to Media-Proxy.");
        return NULL;
    }

    /* Create media-proxy session thru socket. */
    ret = media_proxy_create_session(sockfd, mc, conn, &session_id);
    if (ret) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to create network connection to Media-Proxy.");
        close_socket(sockfd);
        return NULL;
    }

    /* Query memif info from media-proxy. */
    ret = media_proxy_query_interface(sockfd, session_id, mc, conn, &memif_param);
    if (ret) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to query interface from Media-Proxy.");
        close_socket(sockfd);
        return NULL;
    }

    /* Connect memif connection. */
    ret = mcm_create_connection_memif(mc, conn, &memif_param);
    if (ret) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to create memif interface.");
        close_socket(sockfd);
        return NULL;
    }

    conn->proxy_sockfd = sockfd;
    conn->session_id = session_id;

    mesh_log(mc, MESH_LOG_VERBOSE, ("Connected to MCM Media-Proxy");
    return NULL;
}

/*****************************************************
 * Create network connection.
 ******************************************************/

/* Create a new mesh connection */
int mesh_create_connection(MeshClient *mc, MeshConnection *conn, MeshConnectionConfig *cfg);
{
    conn = calloc(1, siezof(MeshConnectionConfig));
    if (mc==NULL) return MESH_CANNOT_CREATE_MESH_CONNECTION;

    memcpy(conn, cfg, sizeof(MeshClientConfig));   

    void* p_ctx = NULL;
    mcm_conn_context* conn_ctx = NULL;

    if (param == NULL) {
        mesh_log(mc, "Illegal Parameters.");
        return NULL;
    }

    /* Create connection w/o media-proxy. */
    switch (param->protocol) {
    case PROTO_AUTO:
        /* Connect to MCM media proxy daemon through memif. */
        if (mcm_create_connection_proxy(mc, conn)) {
            mesh_log(mc, MESH_LOG_ERROR, "Fail to connect MCM media-proxy.");
            return MESH_CANNOT_CREATE_MESH_CONNECTION;
        } else {
            mesh_log(mc, MESH_LOG_VERBOSE, "Success connect to MCM media-proxy.");
        }
        conn->proto = PROTO_AUTO;
        break;
    case PROTO_UDP:
        if (mcm_create_connection_proxy(mc, conn)) {
            mesh_log(mc, MESH_LOG_ERROR, "Fail to create UDP connection.");
            return MESH_CANNOT_CREATE_MESH_CONNECTION;
        }
        conn->proto = PROTO_UDP;
        break;
    case PROTO_MEMIF:
        memif_conn_param memif_param = {};
        parse_memif_param(conn, &(memif_param.socket_args), &(memif_param.conn_args));
        /* Connect memif connection. */
        if (mcm_create_connection_memif(mc, conn, &memif_param)) {
            mesh_log(mc, MESH_LOG_ERROR, "Failed to create memif connection.");
            return MESH_CANNOT_CREATE_MESH_CONNECTION;
        }
        conn_ctx->session_id = 0;
        break;
    default:
        mesh_log(mc, MESH_LOG_ERROR, "Unsupported protocol: %d", param->protocol);
        break;
    }

    /* common settings for all protocols. */
    conn->type = param->type;
    return NULL;
}

/* Destroy MCM DP connection. */
int mesh_delete_connection(MeshClient *mc, MeshConnection *conn);
{
    useconds_t wait_time = 0;

    if (conn == NULL) {
        return MESH_ERR_BAD_CONNECTION_HANDLE;
    }

    if (conn->type == is_tx) {
        if (conn->fps > 0) {
            uint32_t tmp_val = 0;
            /* Wait for time of send 20 frames in current FPS. */
            tmp_val = 1000000 / pctx->fps;
            tmp_val *= 20;
            wait_time = (useconds_t)tmp_val;
        } else {
            /* Wait for 1 second if didn't set FPS. */
            wait_time = 1000000;
        }
        usleep(wait_time);
    }

    if (conn->proxy_sockfd > 0) {
        media_proxy_destroy_session(mc, conn);
    }

    switch (conn->proto) {
    case PROTO_MEMIF:
        mcm_destroy_connection_memif((memif_conn_context*)conn->priv);
        break;
    case PROTO_UDP:
        mcm_destroy_connection_udp((udp_context*)conn->priv);
        break;
    default:
        mesh_log(mc, MESH_LOG_ERROR, "Unsupported protocol: %d", conn->proto);
        break;
    }

    free(pctx);
}

int mesh_get_buffer(MeshClient mc, MeshConnection conn, MeshBuffer buf, MeshBufferInfo *info, int timeout_ms)
{
    return dequeue_buffer(mc, conn, buf, timeout_ms);
}

int mesh_put_buffer(MeshClient mc, MeshConnection conn, MeshBuffer buf, int timeout_ms)
{
    return enqueue_buffer(mc, conn, buf, timeout_ms);
}
