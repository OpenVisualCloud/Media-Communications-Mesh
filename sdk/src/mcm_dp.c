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
    mc = calloc(1, sizeof(MeshClientConfig));
    if (mc==NULL) return MESH_CANNOT_CREATE_MESH_CLIENT;

    memcpy(mc, cfg, sizeof(MeshClientConfig));   
    return 0;
}

/* Delete mesh client */
int mesh_delete_client(MeshClient *mc)
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
int mcm_create_connection_proxy(MeshClient mc, MeshConnection conn)
{
    int ret = 0;
    int sockfd = 0;
    uint32_t session_id = 0;
    memif_conn_param memif_param = {};

    MeshClientConfig *mc_conf = (MeshClientConfig*) mc;
    MeshConnectionConfig *conn_conf= (MeshConnectionConfig*) conn;

    mcm_conn_param param;
    param.type = conn_conf->type;
    param.protocol = conn_conf->proto;
    param.local_addr = conn_conf->local_addr;
    param.remote_addr = conn_conf->remote_addr;
    param.memif_interface = conn_conf->memif_interface;
    param.payload_type = conn_conf->payload_type;
    param.payload_codec = conn_conf->payload_codec;
    param.payload_args.video_args = conn_conf->payload_args.video_args;
    param.payload_args.audio_args = conn_conf->payload_args.audio_args;
    param.payload_args.anc_args = conn_conf->payload_args.anc_args;
    param.width = conn_conf->width;
    param.height = conn_conf->height;
    param.fps = conn_conf->fps;
    param.pix_fmt = conn_conf->pix_fmt;
    param.payload_type_nr = conn_conf->payload_type_nr;
    param.payload_mtl_flags_mask = conn_conf->payload_mtl_flags_mask;
    param.payload_mtl_pacing = conn_conf->payload_mtl_pacing;

    /* Get media-proxy address. */
    ret = get_media_proxy_addr(mc, mc_conf->proxy_addr);
    if (ret != 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to get MCM Media-Proxy address.");
        return MESH_CANNOT_CREATE_MESH_CONNECTION;
    }

//TBD    mesh_log(mc, MESH_LOG_INFO, ("Connecting to MCM Media-Proxy: %s:%s", conn_conf->proxy_addr->ip, conn_conf->proxy_addr->ip));

    /* Create socket connection with media-proxy. */
    sockfd = open_socket(mc, mc_conf->proxy_addr);
    if (sockfd < 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to create network connection to Media-Proxy.");
        return MESH_CANNOT_CREATE_MESH_CONNECTION;
    }

    /* Create media-proxy session thru socket. */
    ret = media_proxy_create_session(mc, &param, sockfd, &session_id);
    if (ret) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to create network connection to Media-Proxy.");
        close_socket(sockfd);
        return MESH_CANNOT_CREATE_MESH_CONNECTION;
    }

    /* Query memif info from media-proxy. */
    ret = media_proxy_query_interface(mc, sockfd, session_id, &param, &memif_param);
    if (ret) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to query interface from Media-Proxy.");
        close_socket(sockfd);
        return MESH_CANNOT_CREATE_MESH_CONNECTION;
    }

    /* Connect memif connection. */
    ret = mcm_create_connection_memif(mc, conn, &param, &memif_param);
    if (ret) {
        mesh_log(mc, MESH_LOG_ERROR, "Fail to create memif interface.");
        close_socket(sockfd);
        return MESH_CANNOT_CREATE_MESH_CONNECTION;
    }

    conn_conf->proxy_sockfd = sockfd;
    conn_conf->session_id = session_id;

    mesh_log(mc, MESH_LOG_INFO, ("Connected to MCM Media-Proxy"));
    return MCM_DP_SUCCESS;
}

/*****************************************************
 * Create network connection.
 ******************************************************/

/* Create a new mesh connection */
int mesh_create_connection(MeshClient mc, MeshConnection conn, mcm_conn_param* param)
{
    conn = calloc(1, sizeof(MeshConnectionConfig));
    if (mc==NULL) return MESH_CANNOT_CREATE_MESH_CONNECTION;

    MeshConnectionConfig *conn_conf= (MeshConnectionConfig*) conn;

    conn_conf->type = param->type;
    conn_conf->proto = param->protocol;
    conn_conf->local_addr = param->local_addr;
    conn_conf->remote_addr = param->remote_addr;
    conn_conf->memif_interface = param->memif_interface;
    conn_conf->payload_type = param->payload_type;
    conn_conf->payload_codec = param->payload_codec;
    conn_conf->payload_args.video_args = param->payload_args.video_args;
    conn_conf->payload_args.audio_args = param->payload_args.audio_args;
    conn_conf->payload_args.anc_args = param->payload_args.anc_args;
    conn_conf->width = param->width;
    conn_conf->height = param->height;
    conn_conf->fps = param->fps;
    conn_conf->pix_fmt = param->pix_fmt;
    conn_conf->payload_type_nr = param->payload_type_nr;
    conn_conf->payload_mtl_flags_mask = param->payload_mtl_flags_mask;
    conn_conf->payload_mtl_pacing = param->payload_mtl_pacing;

    if (param == NULL) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal Parameters.");
        return MCM_DP_ERROR_INVALID_PARAM;
    }

    /* Create connection w/o media-proxy. */
    switch (param->protocol) {
    case PROTO_AUTO:
        /* Connect to MCM media proxy daemon through memif. */
        if (mcm_create_connection_proxy(mc, conn)) {
            mesh_log(mc, MESH_LOG_ERROR, "Fail to connect MCM media-proxy.");
            return MESH_CANNOT_CREATE_MESH_CONNECTION;
        } else {
            mesh_log(mc, MESH_LOG_INFO, "Success connect to MCM media-proxy.");
        }
        conn_conf->proto = PROTO_AUTO;
        break;
    case PROTO_UDP:
        if (mcm_create_connection_proxy(mc, conn)) {
            mesh_log(mc, MESH_LOG_ERROR, "Fail to create UDP connection.");
            return MESH_CANNOT_CREATE_MESH_CONNECTION;
        }
        conn_conf->proto = PROTO_UDP;
        break;
    case PROTO_MEMIF:
        memif_conn_param memif_param = {};
        parse_memif_param(conn, &(memif_param.socket_args), &(memif_param.conn_args));
        /* Connect memif connection. */
        if (mcm_create_connection_memif(mc, conn, param, &memif_param)) {
            mesh_log(mc, MESH_LOG_ERROR, "Failed to create memif connection.");
            return MESH_CANNOT_CREATE_MESH_CONNECTION;
        }
        conn_conf->session_id = 0;
        break;
    default:
        mesh_log(mc, MESH_LOG_ERROR, "Unsupported protocol: %d", param->protocol);
        break;
    }

    /* common settings for all protocols. */
    conn_conf->type = param->type;
    return MCM_DP_SUCCESS;
}

/* Destroy MCM DP connection. */
int mesh_delete_connection(MeshClient mc, MeshConnection conn)
{
    MeshConnectionConfig *conn_conf= (MeshConnectionConfig*) conn;

    useconds_t wait_time = 0;

    if (conn_conf == NULL) {
        return MESH_ERR_BAD_CONNECTION_HANDLE;
    }

    if (conn_conf->type == is_tx) {
        if (conn_conf->fps > 0) {
            uint32_t tmp_val = 0;
            /* Wait for time of send 20 frames in current FPS. */
            tmp_val = 1000000 / conn_conf->fps;
            tmp_val *= 20;
            wait_time = (useconds_t)tmp_val;
        } else {
            /* Wait for 1 second if didn't set FPS. */
            wait_time = 1000000;
        }
        usleep(wait_time);
    }

    if (conn_conf->proxy_sockfd > 0) {
        media_proxy_destroy_session(mc, conn);
    }

    switch (conn_conf->proto) {
    case PROTO_MEMIF:
        mcm_destroy_connection_memif(mc, (memif_conn_context*)conn_conf->priv);
        break;
    case PROTO_UDP:
        mcm_destroy_connection_udp((udp_context*)conn_conf->priv);
        break;
    default:
        mesh_log(mc, MESH_LOG_ERROR, "Unsupported protocol: %d", conn_conf->proto);
        break;
    }

    free(conn);
    return MCM_DP_SUCCESS;
}

int mesh_get_buffer(MeshClient mc, MeshConnection conn, MeshBuffer buf, int timeout_ms)
{
    return memif_dequeue_buffer(mc, conn, buf, timeout_ms);
}

int mesh_put_buffer(MeshClient mc, MeshConnection conn, MeshBuffer buf, int timeout_ms)
{
    return memif_enqueue_buffer(mc, conn, buf, timeout_ms);
}

void mesh_log(MeshClient mc, int log_level, ...)
{    

}
