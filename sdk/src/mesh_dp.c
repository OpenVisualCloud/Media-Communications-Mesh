/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <pthread.h>
#include "mesh_dp_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <bsd/string.h>
#include <stdatomic.h>
#include <unistd.h>

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct mesh_internal_ops_t mesh_internal_ops = {
    .create_conn = mcm_create_connection,
    .destroy_conn = mcm_destroy_connection,
    .dequeue_buf = mcm_dequeue_buffer,
    .enqueue_buf = mcm_enqueue_buffer,
};

/**
 * Connection list head type definition
 */
LIST_HEAD(MeshConnectionListHead, MeshConnectionContext);

/**
 * Mesh client context structure
 */
typedef struct MeshClientContext {
    MeshClientConfig config;

    struct MeshConnectionListHead conn_list_head;
    pthread_mutex_t mx;

    atomic_int conn_num;

} MeshClientContext;

/**
 * Mesh connection buffer structure
 */
typedef struct MeshBufferContext {
    /**
     * NOTE: The __public structure is directly mapped in the memory to the
     * MeshBuffer structure, which is publicly accessible to the user.
     * Therefore, the __public structure _MUST_ be placed first here.
     */
    MeshBuffer __public;

    /**
     * NOTE: All declarations below this point are hidden from the user.
     */
    mcm_buffer *buf;

} MeshBufferContext;

/**
 * Create a new mesh client
 */
int mesh_create_client(MeshClient **mc, MeshClientConfig *cfg)
{
    MeshClientContext *mc_ctx;
    int err;

    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    mc_ctx = mesh_calloc(sizeof(MeshClientContext));
    if (!mc_ctx) {
        *mc = NULL;
        return -ENOMEM;
    }

    atomic_init(&mc_ctx->conn_num, 0);

    err = pthread_mutex_init(&mc_ctx->mx, NULL);
    if (err) {
        mesh_free(mc_ctx);
        *mc = NULL;
        return err;
    }

    LIST_INIT(&mc_ctx->conn_list_head);

    if (cfg)
        mesh_memcpy(&mc_ctx->config, cfg, sizeof(MeshClientConfig));

    if (mc_ctx->config.max_conn_num == 0)
        mc_ctx->config.max_conn_num = MESH_CLIENT_DEFAULT_MAX_CONN;

    if (mc_ctx->config.timeout_ms == 0)
        mc_ctx->config.timeout_ms = MESH_CLIENT_DEFAULT_TIMEOUT_MS;

    *mc = (MeshClient *)mc_ctx;

    return 0;
}

/**
 * Delete mesh client
 */
int mesh_delete_client(MeshClient **mc)
{
    MeshClientContext *mc_ctx;

    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    mc_ctx = (MeshClientContext *)(*mc);

    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    pthread_mutex_destroy(&mc_ctx->mx);

    /**
     * TODO: Shutdown and deallocate connections here
     */

    mesh_free(mc_ctx);
    *mc = NULL;

    return 0;
}

/**
 * Create a new mesh connection
 */
int mesh_create_connection(MeshClient *mc, MeshConnection **conn)
{
    MeshConnectionContext *conn_ctx;
    MeshClientContext *mc_ctx;
    int conn_num, err;

    if (!mc)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    mc_ctx = (MeshClientContext *)mc;

    conn_num = atomic_load_explicit(&mc_ctx->conn_num, memory_order_relaxed);
    while (conn_num < mc_ctx->config.max_conn_num) {
        if (atomic_compare_exchange_weak_explicit(&mc_ctx->conn_num, &conn_num,
                                                  conn_num + 1,
                                                  memory_order_release,
                                                  memory_order_relaxed))
            break;
    }
    if (!(conn_num < mc_ctx->config.max_conn_num))
        return -MESH_ERR_MAX_CONN;

    conn_ctx = mesh_calloc(sizeof(MeshConnectionContext));
    if (!conn_ctx) {
        err = -ENOMEM;
        goto exit_dec_conn_num;
    }

    *(MeshClient **)&conn_ctx->__public.client = mc;

    conn_ctx->cfg.conn_type = MESH_CONN_TYPE_UNINITIALIZED;
    conn_ctx->cfg.payload_type = MESH_PAYLOAD_TYPE_UNINITIALIZED;

    err = pthread_mutex_lock(&mc_ctx->mx);
    if (err)
        goto exit_dealloc_conn;

    LIST_INSERT_HEAD(&mc_ctx->conn_list_head, conn_ctx, conns);

    pthread_mutex_unlock(&mc_ctx->mx);

    *conn = (MeshConnection *)conn_ctx;

    return 0;

exit_dealloc_conn:
    mesh_free(conn_ctx);

exit_dec_conn_num:
    atomic_fetch_sub(&mc_ctx->conn_num, 1);

    return err;
}

/**
 * Apply configuration to setup Single node direct connection via memif
 */
int mesh_apply_connection_config_memif(MeshConnection *conn, MeshConfig_Memif *cfg)
{
    MeshConnectionContext *conn_ctx;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    conn_ctx = (MeshConnectionContext *)conn;
    conn_ctx->cfg.conn_type = MESH_CONN_TYPE_MEMIF;
    mesh_memcpy(&conn_ctx->cfg.conn.memif, cfg, sizeof(MeshConfig_Memif));

    return 0;
}

/**
 * Apply configuration to setup SMPTE ST2110-xx connection via Media Proxy
 */
int mesh_apply_connection_config_st2110(MeshConnection *conn, MeshConfig_ST2110 *cfg)
{
    MeshConnectionContext *conn_ctx;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    conn_ctx = (MeshConnectionContext *)conn;
    conn_ctx->cfg.conn_type = MESH_CONN_TYPE_ST2110;
    mesh_memcpy(&conn_ctx->cfg.conn.st2110, cfg, sizeof(MeshConfig_ST2110));

    return 0;
}

/**
 * Apply configuration to setup RDMA connection via Media Proxy
 */
int mesh_apply_connection_config_rdma(MeshConnection *conn, MeshConfig_RDMA *cfg)
{
    MeshConnectionContext *conn_ctx;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    conn_ctx = (MeshConnectionContext *)conn;
    conn_ctx->cfg.conn_type = MESH_CONN_TYPE_RDMA;
    mesh_memcpy(&conn_ctx->cfg.conn.rdma, cfg, sizeof(MeshConfig_RDMA));

    return 0;
}

/**
 * Apply configuration to setup connection payload for Video frames
 */
int mesh_apply_connection_config_video(MeshConnection *conn, MeshConfig_Video *cfg)
{
    MeshConnectionContext *conn_ctx;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    conn_ctx = (MeshConnectionContext *)conn;
    conn_ctx->cfg.payload_type = MESH_PAYLOAD_TYPE_VIDEO;
    mesh_memcpy(&conn_ctx->cfg.payload.video, cfg, sizeof(MeshConfig_Video));

    return 0;
}

/**
 * Apply configuration to setup connection payload for Audio packets
 */
int mesh_apply_connection_config_audio(MeshConnection *conn, MeshConfig_Audio *cfg)
{
    MeshConnectionContext *conn_ctx;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (!cfg)
        return -MESH_ERR_BAD_CONFIG_PTR;

    conn_ctx = (MeshConnectionContext *)conn;
    conn_ctx->cfg.payload_type = MESH_PAYLOAD_TYPE_AUDIO;
    mesh_memcpy(&conn_ctx->cfg.payload.audio, cfg, sizeof(MeshConfig_Audio));

    return 0;
}

/**
 * Parse payload configuration
 */
int mesh_parse_payload_config(MeshConnectionContext *conn, mcm_conn_param *param)
{
    /**
     * Parse video payload parameters
     */
    if (conn->cfg.payload_type == MESH_PAYLOAD_TYPE_VIDEO) {

        switch (conn->cfg.payload.video.pixel_format) {
        case MESH_VIDEO_PIXEL_FORMAT_NV12:
            param->pix_fmt = PIX_FMT_NV12;
            break;            
        case MESH_VIDEO_PIXEL_FORMAT_YUV422P:
            param->pix_fmt = PIX_FMT_YUV422P;
            break;            
        case MESH_VIDEO_PIXEL_FORMAT_YUV422P10LE:
            param->pix_fmt = PIX_FMT_YUV422P_10BIT_LE;
            break;
        case MESH_VIDEO_PIXEL_FORMAT_YUV444P10LE:
            param->pix_fmt = PIX_FMT_YUV444P_10BIT_LE;
            break;
        case MESH_VIDEO_PIXEL_FORMAT_RGB8:
            param->pix_fmt = PIX_FMT_RGB8;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        param->payload_args.video_args.pix_fmt = param->pix_fmt;
        param->payload_args.video_args.width   = conn->cfg.payload.video.width;
        param->width                           = conn->cfg.payload.video.width;
        param->payload_args.video_args.height  = conn->cfg.payload.video.height;
        param->height                          = conn->cfg.payload.video.height;
        param->payload_args.video_args.fps     = param->fps = conn->cfg.payload.video.fps;

        return 0;
    }

    /**
     * Parse audio payload parameters
     */
    if (conn->cfg.payload_type == MESH_PAYLOAD_TYPE_AUDIO) {

        switch (conn->cfg.payload.audio.sample_rate) {
        case MESH_AUDIO_SAMPLE_RATE_44100:
            param->payload_args.audio_args.sampling = AUDIO_SAMPLING_44K;
            break; 
        case MESH_AUDIO_SAMPLE_RATE_48000:
            param->payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
            break;
        case MESH_AUDIO_SAMPLE_RATE_96000:
            param->payload_args.audio_args.sampling = AUDIO_SAMPLING_96K;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        switch (conn->cfg.payload.audio.sample_rate) {
        case MESH_AUDIO_SAMPLE_RATE_48000:
        case MESH_AUDIO_SAMPLE_RATE_96000:
            switch (conn->cfg.payload.audio.packet_time) {
            case MESH_AUDIO_PACKET_TIME_1MS:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_1MS;
                break;
            case MESH_AUDIO_PACKET_TIME_125US:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_125US;
                break;
            case MESH_AUDIO_PACKET_TIME_250US:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_250US;
                break;
            case MESH_AUDIO_PACKET_TIME_333US:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_333US;
                break;
            case MESH_AUDIO_PACKET_TIME_4MS:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_4MS;
                break;
            case MESH_AUDIO_PACKET_TIME_80US:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_80US;
                break;
            default:
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;
            }
            break;
        case MESH_AUDIO_SAMPLE_RATE_44100:
            switch (conn->cfg.payload.audio.packet_time) {
            case MESH_AUDIO_PACKET_TIME_1_09MS:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_1_09MS;
                break;
            case MESH_AUDIO_PACKET_TIME_0_14MS:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_0_14MS;
                break;
            case MESH_AUDIO_PACKET_TIME_0_09MS:
                param->payload_args.audio_args.ptime = AUDIO_PTIME_0_09MS;
                break;
            default:
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;
            }
            break;
        default:
            /**
             * Default case cannot happen here by design.
             */
        }

        switch (conn->cfg.payload.audio.format) {
        case MESH_AUDIO_FORMAT_PCM_S8:
            param->payload_args.audio_args.format = AUDIO_FMT_PCM8;
            break;
        case MESH_AUDIO_FORMAT_PCM_S16BE:
            param->payload_args.audio_args.format = AUDIO_FMT_PCM16;
            break;
        case MESH_AUDIO_FORMAT_PCM_S24BE:
            param->payload_args.audio_args.format = AUDIO_FMT_PCM24;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        param->payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
        param->payload_args.audio_args.channel = conn->cfg.payload.audio.channels;

        return 0;
    }

    return -MESH_ERR_CONN_CONFIG_INVAL;
}

/**
 * Parse connection configuration and check for for wrong or incompatible
 * parameter values.
 */
int mesh_parse_conn_config(MeshConnectionContext *ctx, mcm_conn_param *param)
{
    switch (ctx->cfg.kind) {
    case MESH_CONN_KIND_SENDER:
        param->type = is_tx;
        break;
    case MESH_CONN_KIND_RECEIVER:
        param->type = is_rx;
        break;
    default:
        return -MESH_ERR_CONN_CONFIG_INVAL;
    }

    switch (ctx->cfg.conn_type) {
    /**
     * Parse parameters of memif connection.
     */
    case MESH_CONN_TYPE_MEMIF:
        param->protocol = PROTO_MEMIF;

        mesh_strlcpy(param->memif_interface.socket_path,
                     ctx->cfg.conn.memif.socket_path,
                     sizeof(param->memif_interface.socket_path));

        param->memif_interface.interface_id = ctx->cfg.conn.memif.interface_id;
        param->memif_interface.is_master = (param->type == is_tx) ? 1 : 0;

        if (ctx->cfg.payload_type == MESH_PAYLOAD_TYPE_VIDEO)
            param->payload_type = PAYLOAD_TYPE_ST20_VIDEO;
        else if (ctx->cfg.payload_type == MESH_PAYLOAD_TYPE_AUDIO)
            param->payload_type = PAYLOAD_TYPE_ST30_AUDIO;
        break;
    /**
     * Parse and check parameters of SMPTE ST2110-XX connection.
     */
    case MESH_CONN_TYPE_ST2110:
        param->protocol = PROTO_AUTO;

        mesh_strlcpy(param->local_addr.ip, ctx->cfg.conn.st2110.local_ip_addr,
                     sizeof(param->local_addr.ip));
        snprintf(param->local_addr.port, sizeof(param->local_addr.port),
                 "%u", ctx->cfg.conn.st2110.local_port);

        mesh_strlcpy(param->remote_addr.ip, ctx->cfg.conn.st2110.remote_ip_addr,
                     sizeof(param->remote_addr.ip));
        snprintf(param->remote_addr.port, sizeof(param->remote_addr.port),
                 "%u", ctx->cfg.conn.st2110.remote_port);

        switch (ctx->cfg.conn.st2110.transport) {
        /**
         * SMPTE ST2110-20 Uncompressed video transport
         */
        case MESH_CONN_TRANSPORT_ST2110_20:
            if (ctx->cfg.payload_type != MESH_PAYLOAD_TYPE_VIDEO)
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;

            param->payload_type = PAYLOAD_TYPE_ST20_VIDEO;
            break;
        /**
         * SMPTE ST2110-22 Constant Bit-Rate Compressed Video transport
         */
        case MESH_CONN_TRANSPORT_ST2110_22:
            if (ctx->cfg.payload_type != MESH_PAYLOAD_TYPE_VIDEO)
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;

            param->payload_type = PAYLOAD_TYPE_ST22_VIDEO;
            param->payload_codec = PAYLOAD_CODEC_JPEGXS;
            break;
        /**
         * SMPTE ST2110-30 Audio transport
         */
        case MESH_CONN_TRANSPORT_ST2110_30:
            if (ctx->cfg.payload_type != MESH_PAYLOAD_TYPE_AUDIO)
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;

            param->payload_type = PAYLOAD_TYPE_ST30_AUDIO;
            break;
        /**
         * Unknown transport
         */
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }
        break;
    /**
     * Parse parameters of RDMA connection.
     */
    case MESH_CONN_TYPE_RDMA:
        param->protocol = PROTO_AUTO;

        mesh_strlcpy(param->local_addr.ip, ctx->cfg.conn.rdma.local_ip_addr,
                     sizeof(param->local_addr.ip));
        snprintf(param->local_addr.port, sizeof(param->local_addr.port),
                 "%u", ctx->cfg.conn.rdma.local_port);

        mesh_strlcpy(param->remote_addr.ip, ctx->cfg.conn.rdma.remote_ip_addr,
                     sizeof(param->remote_addr.ip));
        snprintf(param->remote_addr.port, sizeof(param->remote_addr.port),
                 "%u", ctx->cfg.conn.rdma.remote_port);

        /**
         * TODO: Rework this to enable both video and audio payloads.
         */
        param->payload_type = PAYLOAD_TYPE_RDMA_VIDEO;
        break;
    /**
     * Unknown connection type
     */
    default:
        return -MESH_ERR_CONN_CONFIG_INVAL;
    }

    return 0;
}

/**
 * Establish a mesh connection
 */
int mesh_establish_connection(MeshConnection *conn, int kind)
{
    MeshConnectionContext *conn_ctx;
    mcm_conn_param param = { 0 };
    MeshClientContext *mc_ctx;
    int err;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    conn_ctx = (MeshConnectionContext *)conn;

    if (conn_ctx->handle)
        return -MESH_ERR_BAD_CONN_PTR;

    mc_ctx = (MeshClientContext *)conn_ctx->__public.client;
    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if (conn_ctx->cfg.conn_type == MESH_CONN_TYPE_UNINITIALIZED ||
        conn_ctx->cfg.payload_type == MESH_PAYLOAD_TYPE_UNINITIALIZED)
        return -MESH_ERR_CONN_CONFIG_INVAL;

    conn_ctx->cfg.kind = kind;

    err = mesh_parse_payload_config(conn_ctx, &param);
    if (err)
        return err;

    err = mesh_parse_conn_config(conn_ctx, &param);
    if (err)
        return err;

    conn_ctx->handle = mesh_internal_ops.create_conn(&param);
    if (!conn_ctx->handle)
        return -MESH_ERR_CONN_FAILED;

    *(size_t *)&conn_ctx->__public.buf_size = conn_ctx->handle->frame_size;

    return 0;
}

/**
 * Shutdown a mesh connection
 */
int mesh_shutdown_connection(MeshConnection *conn)
{
    MeshConnectionContext *conn_ctx;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    conn_ctx = (MeshConnectionContext *)conn;

    if (conn_ctx->handle) {
        /** In Sender mode, delay for 50ms to allow for completing
         * transmission of all buffers sitting in the memif queue
         * before destroying the connection.
         *
         * TODO: Replace the delay with polling of the actual memif
         * queue status.
         */
        if (conn_ctx->cfg.kind == MESH_CONN_KIND_SENDER)
            usleep(50000);

        mesh_internal_ops.destroy_conn(conn_ctx->handle);
        conn_ctx->handle = NULL;
    }

    return 0;
}

/**
 * Delete mesh connection
 */
int mesh_delete_connection(MeshConnection **conn)
{
    MeshConnectionContext *conn_ctx;
    MeshClientContext *mc_ctx;
    int err;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    err = mesh_shutdown_connection(*conn);
    if (err)
        return err;

    conn_ctx = (MeshConnectionContext *)(*conn);

    if (conn_ctx->handle) {
        mesh_internal_ops.destroy_conn(conn_ctx->handle);
        conn_ctx->handle = NULL;
    }

    mc_ctx = (MeshClientContext *)conn_ctx->__public.client;
    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    err = pthread_mutex_lock(&mc_ctx->mx);
    if (err)
        return err;

    LIST_REMOVE(conn_ctx, conns);

    pthread_mutex_unlock(&mc_ctx->mx);

    atomic_fetch_sub(&mc_ctx->conn_num, 1);

    mesh_free(conn_ctx);
    *conn = NULL;

    return 0;
}

/**
 * Get buffer from mesh connection
 */
inline int mesh_get_buffer(MeshConnection *conn, MeshBuffer **buf)
{
    return mesh_get_buffer_timeout(conn, buf, MESH_TIMEOUT_DEFAULT);
}

/**
 * Get buffer from mesh connection with timeout
 */
int mesh_get_buffer_timeout(MeshConnection *conn, MeshBuffer **buf,
                            int timeout_ms)
{
    MeshConnectionContext *conn_ctx;
    MeshBufferContext *buf_ctx;
    int err;

    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    conn_ctx = (MeshConnectionContext *)conn;

    buf_ctx = mesh_calloc(sizeof(MeshBufferContext));
    
    if (!buf_ctx)
        return -ENOMEM;

    if (timeout_ms == MESH_TIMEOUT_DEFAULT && conn && conn->client)
        timeout_ms = ((MeshClientContext *)conn->client)->config.timeout_ms;

    buf_ctx->buf = mesh_internal_ops.dequeue_buf(conn_ctx->handle, timeout_ms, &err);
    if (!buf_ctx->buf) {
        mesh_free(buf_ctx);

        if (!err)
            return -MESH_ERR_CONN_CLOSED;

        return err;
    }

    *(MeshConnection **)&buf_ctx->__public.conn = conn;
    *(void **)&buf_ctx->__public.data = buf_ctx->buf->data;
    *(size_t *)&buf_ctx->__public.data_len = buf_ctx->buf->len;

    *buf = (MeshBuffer *)buf_ctx;

    return 0;
}

/**
 * Put buffer to mesh connection
 */
inline int mesh_put_buffer(MeshBuffer **buf)
{
    return mesh_put_buffer_timeout(buf, MESH_TIMEOUT_DEFAULT);
}

/**
 * Put buffer to mesh connection with timeout
 */
int mesh_put_buffer_timeout(MeshBuffer **buf, int timeout_ms)
{
    MeshBufferContext *buf_ctx;
    mcm_conn_context *handle;
    int err;

    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    buf_ctx = (MeshBufferContext *)(*buf);

    if (!buf_ctx)
        return -MESH_ERR_BAD_BUF_PTR;

    /**
     * TODO: Add timeout handling
     */

    handle = ((MeshConnectionContext *)(buf_ctx->__public.conn))->handle;
    err = mesh_internal_ops.enqueue_buf(handle, buf_ctx->buf);
    if (err)
        return err;

    mesh_free(buf_ctx);
    *buf = NULL;

    return 0;
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
    default:
        return "Unknown error code";
    }
}
