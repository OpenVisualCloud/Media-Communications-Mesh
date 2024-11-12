/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <bsd/string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "memif_impl.h"
#include "udp_impl.h"
#include "logger.h"
#include "mcm_dp.h"
#include "media_proxy_ctrl.h"

/* Calculate audio buffer size based on ST2110-30 related parameters */
static int mcm_calc_audio_buffer_size(mcm_audio_args *params, uint32_t *size)
{
    uint32_t sample_size = 0;
    uint32_t sample_num = 0;

    switch (params->format) {
    case AUDIO_FMT_PCM8:
        sample_size = 1;
        break;
    case AUDIO_FMT_PCM16:
        sample_size = 2;
        break;
    case AUDIO_FMT_PCM24:
        sample_size = 3;
        break;
    case AUDIO_FMT_AM824:
        sample_size = 4;
        break;
    default:
        *size = 0;
        return -EINVAL;
    }

    switch (params->sampling) {
    case AUDIO_SAMPLING_48K:
        switch (params->ptime) {
        case AUDIO_PTIME_1MS:
            sample_num = 48;
            break;
        case AUDIO_PTIME_125US:
            sample_num = 6;
            break;
        case AUDIO_PTIME_250US:
            sample_num = 12;
            break;
        case AUDIO_PTIME_333US:
            sample_num = 16;
            break;
        case AUDIO_PTIME_4MS:
            sample_num = 192;
            break;
        case AUDIO_PTIME_80US:
            sample_num = 4;
            break;
        default:
            *size = 0;
            return -EINVAL;
        }
        break;
    case AUDIO_SAMPLING_96K:
        switch (params->ptime) {
        case AUDIO_PTIME_1MS:
            sample_num = 96;
            break;
        case AUDIO_PTIME_125US:
            sample_num = 12;
            break;
        case AUDIO_PTIME_250US:
            sample_num = 24;
            break;
        case AUDIO_PTIME_333US:
            sample_num = 32;
            break;
        case AUDIO_PTIME_4MS:
            sample_num = 384;
            break;
        case AUDIO_PTIME_80US:
            sample_num = 8;
            break;
        default:
            *size = 0;
            return -EINVAL;
        }
        break;
    case AUDIO_SAMPLING_44K:
        switch (params->ptime) {
        case AUDIO_PTIME_1_09MS:
            sample_num = 48;
            break;
        case AUDIO_PTIME_0_14MS:
            sample_num = 6;
            break;
        case AUDIO_PTIME_0_09MS:
            sample_num = 4;
            break;
        default:
            *size = 0;
            return -EINVAL;
        }
        break;
    default:
        *size = 0;
        return -EINVAL;
    }

    *size = sample_size * sample_num * params->channel;

    return 0;
}

static void parse_memif_param(mcm_conn_param* request, memif_socket_args_t* memif_socket_args, memif_conn_args_t* memif_conn_args)
{
    char type_str[8] = "";
    char interface_name[32];
    int err;

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

    switch (request->payload_type) {
    case PAYLOAD_TYPE_ST30_AUDIO:
        err = mcm_calc_audio_buffer_size(&request->payload_args.audio_args, &memif_conn_args->buffer_size);
        if (err)
            log_error("Invalid audio parameters.");
        break;

    case PAYLOAD_TYPE_ST20_VIDEO:
    case PAYLOAD_TYPE_ST22_VIDEO:
    case PAYLOAD_TYPE_RTSP_VIDEO:
    default:
        memif_conn_args->buffer_size = request->payload_args.video_args.width * request->payload_args.video_args.height * 4;
    }

    memif_conn_args->log2_ring_size = 4;
}

/* Connect to MCM media proxy daemon through memif. */
mcm_conn_context* mcm_create_connection_proxy(mcm_conn_param* param)
{
    int ret = 0;
    mcm_conn_context* conn_ctx = NULL;
    mcm_dp_addr media_proxy_addr = {};
    int sockfd = 0;
    uint32_t session_id = 0;
    memif_conn_param memif_param = {};

    /* Get media-proxy address. */
    ret = get_media_proxy_addr(&media_proxy_addr);
    if (ret != 0) {
        log_warn("Fail to get MCM Media-Proxy address.");
        return NULL;
    }

    log_info("Connecting to MCM Media-Proxy: %s:%s", media_proxy_addr.ip, media_proxy_addr.port);

    /* Create socket connection with media-proxy. */
    sockfd = open_socket(&media_proxy_addr);
    if (sockfd < 0) {
        log_error("Fail to create network connection to Media-Proxy.");
        return NULL;
    }

    /* Create media-proxy session thru socket. */
    ret = media_proxy_create_session(sockfd, param, &session_id);
    if (ret < 0) {
        log_error("Fail to create network connection to Media-Proxy.");
        close_socket(sockfd);
        return NULL;
    }

    /* Query memif info from media-proxy. */
    ret = media_proxy_query_interface(sockfd, session_id, param, &memif_param);
    if (ret < 0) {
        log_error("Fail to query interface from Media-Proxy.");
        close_socket(sockfd);
        return NULL;
    }

    /* Connect memif connection. */
    conn_ctx = mcm_create_connection_memif(param, &memif_param);
    if (conn_ctx == NULL) {
        log_error("Fail to create memif interface.");
        close_socket(sockfd);
        return NULL;
    }

    conn_ctx->proxy_sockfd = sockfd;
    conn_ctx->session_id = session_id;

    return conn_ctx;
}

/*****************************************************
 * Create network connection.
 *
 * @param param Parameters to create the connection with
 *              media-proxy.
 * @return Connection context if success, NULL if failed.
 ******************************************************/
mcm_conn_context* mcm_create_connection(mcm_conn_param* param)
{
    void* p_ctx = NULL;
    mcm_conn_context* conn_ctx = NULL;

    if (param == NULL) {
        log_error("Illegal Parameters.");
        return NULL;
    }

    /* Create connection w/o media-proxy. */
    switch (param->protocol) {
    case PROTO_AUTO:
        /**
         * This is a temporary workaround to calculate the RDMA transfer size
         * from video payload parameters provided by the user. It will be
         * removed after the Control Plane implementation supporting Multipoint
         * Groups is implemented in Media Proxy.
         */
        if (param->payload_type == PAYLOAD_TYPE_RDMA_VIDEO) {
            size_t sz = (size_t)param->payload_args.video_args.width *
                        param->payload_args.video_args.height * 4; 
            param->payload_args.rdma_args.transfer_size = sz;
        }

        /* Connect to MCM media proxy daemon through memif. */
        conn_ctx = mcm_create_connection_proxy(param);
        if (conn_ctx == NULL) {
            log_error("Fail to connect MCM media-proxy.");
            return conn_ctx;
        } else {
            log_info("Success connect to MCM media-proxy.");
        }
        break;
    case PROTO_UDP:
        p_ctx = mcm_create_connection_udp(param);
        if (p_ctx == NULL) {
            log_error("Fail to create UDP connection.");
            return NULL;
        }
        conn_ctx = calloc(1, sizeof(mcm_conn_context));
        if (conn_ctx == NULL) {
            log_error("Outof Memory.");
            free(p_ctx);
            return NULL;
        }
        conn_ctx->proto = PROTO_UDP;
        conn_ctx->priv = (void*)p_ctx;
        break;
    case PROTO_MEMIF:
    {
        memif_conn_param memif_param = {};
        parse_memif_param(param, &(memif_param.socket_args), &(memif_param.conn_args));
        /* Connect memif connection. */
        conn_ctx = mcm_create_connection_memif(param, &memif_param);
        if (!conn_ctx) {
            log_error("Failed to create memif connection.");
            return NULL;
        }
        conn_ctx->session_id = 0;
        break;
    }
    default:
        log_warn("Unsupported protocol: %d", param->protocol);
        break;
    }

    /* common settings for all protocols. */
    if (conn_ctx != NULL) {
        conn_ctx->type = param->type;
    }

    return conn_ctx;
}

/* Destroy MCM DP connection. */
void mcm_destroy_connection(mcm_conn_context* pctx)
{
    useconds_t wait_time = 0;

    if (pctx == NULL) {
        return;
    }

    if (pctx->type == is_tx) {
        if (pctx->fps > 0) {
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

    if (pctx->proxy_sockfd > 0) {
        media_proxy_destroy_session(pctx);
    }

    switch (pctx->proto) {
    case PROTO_MEMIF:
        mcm_destroy_connection_memif((memif_conn_context*)pctx->priv);
        break;
    case PROTO_UDP:
        mcm_destroy_connection_udp((udp_context*)pctx->priv);
        break;
    default:
        log_warn("Unsupported protocol: %d", pctx->proto);
        break;
    }

    free(pctx);
}

mcm_buffer* mcm_dequeue_buffer(mcm_conn_context* pctx, int timeout, int* error_code)
{
    return pctx->dequeue_buffer(pctx, timeout, error_code);
}

int mcm_enqueue_buffer(mcm_conn_context* pctx, mcm_buffer* buf)
{
    return pctx->enqueue_buffer(pctx, buf);
}
