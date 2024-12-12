/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_conn.h"
#include <string.h>
#include <bsd/string.h>
#include <unistd.h>
#include "mesh_buf.h"
#include "mesh_client_api.h"

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct mesh_internal_ops_t mesh_internal_ops = {
    .create_conn = mcm_create_connection,
    .destroy_conn = mcm_destroy_connection,
    .dequeue_buf = mcm_dequeue_buffer,
    .enqueue_buf = mcm_enqueue_buffer,

    .grpc_create_client = mesh_grpc_create_client,
    .grpc_create_client_json = mesh_grpc_create_client_json,
    .grpc_destroy_client = mesh_grpc_destroy_client,
    .grpc_create_conn = mesh_grpc_create_conn,
    .grpc_destroy_conn = mesh_grpc_destroy_conn,
};

namespace mesh {

ConnectionContext::ConnectionContext(ClientContext *parent)
{
    *(MeshClient **)&__public.client = (MeshClient *)parent;

    cfg.conn_type = MESH_CONN_TYPE_UNINITIALIZED;
    cfg.payload_type = MESH_PAYLOAD_TYPE_UNINITIALIZED;
}

int ConnectionContext::apply_config_memif(MeshConfig_Memif *config)
{
    if (!config)
        return -MESH_ERR_BAD_CONFIG_PTR;

    cfg.conn_type = MESH_CONN_TYPE_MEMIF;
    memcpy(&cfg.conn.memif, config, sizeof(MeshConfig_Memif));
    return 0;
}

int ConnectionContext::apply_config_st2110(MeshConfig_ST2110 *config)
{
    if (!config)
        return -MESH_ERR_BAD_CONFIG_PTR;

    cfg.conn_type = MESH_CONN_TYPE_ST2110;
    memcpy(&cfg.conn.st2110, config, sizeof(MeshConfig_ST2110));
    return 0;
}

int ConnectionContext::apply_config_rdma(MeshConfig_RDMA *config)
{
    if (!config)
        return -MESH_ERR_BAD_CONFIG_PTR;

    cfg.conn_type = MESH_CONN_TYPE_RDMA;
    memcpy(&cfg.conn.rdma, config, sizeof(MeshConfig_RDMA));
    return 0;
}

int ConnectionContext::apply_config_video(MeshConfig_Video *config)
{
    if (!config)
        return -MESH_ERR_BAD_CONFIG_PTR;

    cfg.payload_type = MESH_PAYLOAD_TYPE_VIDEO;
    memcpy(&cfg.payload.video, config, sizeof(MeshConfig_Video));
    return 0;
}

int ConnectionContext::apply_config_audio(MeshConfig_Audio *config)
{
    if (!config)
        return -MESH_ERR_BAD_CONFIG_PTR;

    cfg.payload_type = MESH_PAYLOAD_TYPE_AUDIO;
    memcpy(&cfg.payload.audio, config, sizeof(MeshConfig_Audio));
    return 0;
}

int ConnectionContext::parse_payload_config(mcm_conn_param *param)
{
    /**
     * Parse video payload parameters
     */
    if (cfg.payload_type == MESH_PAYLOAD_TYPE_VIDEO) {

        switch (cfg.payload.video.pixel_format) {
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
        param->payload_args.video_args.width   = cfg.payload.video.width;
        param->width                           = cfg.payload.video.width;
        param->payload_args.video_args.height  = cfg.payload.video.height;
        param->height                          = cfg.payload.video.height;
        param->payload_args.video_args.fps     = param->fps = cfg.payload.video.fps;

        return 0;
    }

    /**
     * Parse audio payload parameters
     */
    if (cfg.payload_type == MESH_PAYLOAD_TYPE_AUDIO) {

        switch (cfg.payload.audio.sample_rate) {
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

        switch (cfg.payload.audio.sample_rate) {
        case MESH_AUDIO_SAMPLE_RATE_48000:
        case MESH_AUDIO_SAMPLE_RATE_96000:
            switch (cfg.payload.audio.packet_time) {
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
            switch (cfg.payload.audio.packet_time) {
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
            break;
        }

        switch (cfg.payload.audio.format) {
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
        param->payload_args.audio_args.channel = cfg.payload.audio.channels;

        return 0;
    }

    return -MESH_ERR_CONN_CONFIG_INVAL;
}

int ConnectionContext::parse_conn_config(mcm_conn_param *param)
{
    switch (cfg.kind) {
    case MESH_CONN_KIND_SENDER:
        param->type = is_tx;
        break;
    case MESH_CONN_KIND_RECEIVER:
        param->type = is_rx;
        break;
    default:
        return -MESH_ERR_CONN_CONFIG_INVAL;
    }

    switch (cfg.conn_type) {
    /**
     * Parse parameters of memif connection.
     */
    case MESH_CONN_TYPE_MEMIF:
        param->protocol = PROTO_MEMIF;

        strlcpy(param->memif_interface.socket_path,
                cfg.conn.memif.socket_path,
                sizeof(param->memif_interface.socket_path));

        param->memif_interface.interface_id = cfg.conn.memif.interface_id;
        param->memif_interface.is_master = (param->type == is_tx) ? 1 : 0;

        if (cfg.payload_type == MESH_PAYLOAD_TYPE_VIDEO)
            param->payload_type = PAYLOAD_TYPE_ST20_VIDEO;
        else if (cfg.payload_type == MESH_PAYLOAD_TYPE_AUDIO)
            param->payload_type = PAYLOAD_TYPE_ST30_AUDIO;
        break;
    /**
     * Parse and check parameters of SMPTE ST2110-XX connection.
     */
    case MESH_CONN_TYPE_ST2110:
        param->protocol = PROTO_AUTO;

        strlcpy(param->local_addr.ip, cfg.conn.st2110.local_ip_addr,
                sizeof(param->local_addr.ip));
        snprintf(param->local_addr.port, sizeof(param->local_addr.port),
                 "%u", cfg.conn.st2110.local_port);

        strlcpy(param->remote_addr.ip, cfg.conn.st2110.remote_ip_addr,
                sizeof(param->remote_addr.ip));
        snprintf(param->remote_addr.port, sizeof(param->remote_addr.port),
                 "%u", cfg.conn.st2110.remote_port);

        switch (cfg.conn.st2110.transport) {
        /**
         * SMPTE ST2110-20 Uncompressed video transport
         */
        case MESH_CONN_TRANSPORT_ST2110_20:
            if (cfg.payload_type != MESH_PAYLOAD_TYPE_VIDEO)
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;

            param->payload_type = PAYLOAD_TYPE_ST20_VIDEO;
            break;
        /**
         * SMPTE ST2110-22 Constant Bit-Rate Compressed Video transport
         */
        case MESH_CONN_TRANSPORT_ST2110_22:
            if (cfg.payload_type != MESH_PAYLOAD_TYPE_VIDEO)
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;

            param->payload_type = PAYLOAD_TYPE_ST22_VIDEO;
            param->payload_codec = PAYLOAD_CODEC_JPEGXS;
            break;
        /**
         * SMPTE ST2110-30 Audio transport
         */
        case MESH_CONN_TRANSPORT_ST2110_30:
            if (cfg.payload_type != MESH_PAYLOAD_TYPE_AUDIO)
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

        strlcpy(param->local_addr.ip, cfg.conn.rdma.local_ip_addr,
                sizeof(param->local_addr.ip));
        snprintf(param->local_addr.port, sizeof(param->local_addr.port),
                 "%u", cfg.conn.rdma.local_port);

        strlcpy(param->remote_addr.ip, cfg.conn.rdma.remote_ip_addr,
                sizeof(param->remote_addr.ip));
        snprintf(param->remote_addr.port, sizeof(param->remote_addr.port),
                 "%u", cfg.conn.rdma.remote_port);

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

int ConnectionContext::establish(int kind)
{
    mcm_conn_param param = {};
    int err;

    if (handle)
        return -MESH_ERR_BAD_CONN_PTR;

    ClientContext *mc_ctx = (ClientContext *)__public.client;
    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if (cfg.conn_type == MESH_CONN_TYPE_UNINITIALIZED ||
        cfg.payload_type == MESH_PAYLOAD_TYPE_UNINITIALIZED)
        return -MESH_ERR_CONN_CONFIG_INVAL;

    cfg.kind = kind;

    err = parse_payload_config(&param);
    if (err)
        return err;

    err = parse_conn_config(&param);
    if (err)
        return err;

    if (mc_ctx->config.enable_grpc) {
        grpc_conn = mesh_internal_ops.grpc_create_conn(mc_ctx->grpc_client,
                                                       &param);
        if (!grpc_conn) {
            handle = NULL;
            return -MESH_ERR_CONN_FAILED;
        }
        handle = *(mcm_conn_context **)grpc_conn; // unsafe type casting
        if (!handle)
            return -MESH_ERR_CONN_FAILED;
    } else {
        handle = mesh_internal_ops.create_conn(&param);
        if (!handle)
            return -MESH_ERR_CONN_FAILED;
    }

    *(size_t *)&__public.buf_size = handle->frame_size;

    return 0;
}

int ConnectionContext::shutdown()
{
    ClientContext *mc_ctx = (ClientContext *)__public.client;
    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    if (mc_ctx->config.enable_grpc) {
        if (grpc_conn) {
            /** In Sender mode, delay for 50ms to allow for completing
             * transmission of all buffers sitting in the memif queue
             * before destroying the connection.
             *
             * TODO: Replace the delay with polling of the actual memif
             * queue status.
             */
            if (cfg.kind == MESH_CONN_KIND_SENDER)
                usleep(50000);

            mesh_internal_ops.grpc_destroy_conn(grpc_conn);
            grpc_conn = NULL;
            handle = NULL;
        }
    } else {
        if (handle) {
            /** In Sender mode, delay for 50ms to allow for completing
             * transmission of all buffers sitting in the memif queue
             * before destroying the connection.
             *
             * TODO: Replace the delay with polling of the actual memif
             * queue status.
             */
            if (cfg.kind == MESH_CONN_KIND_SENDER)
                usleep(50000);

            mesh_internal_ops.destroy_conn(handle);
            handle = NULL;
        }
    }

    return 0;
}

ConnectionContext::~ConnectionContext()
{
    ClientContext *mc_ctx = (ClientContext *)__public.client;

    std::lock_guard<std::mutex> lk(mc_ctx->mx);

    mc_ctx->conns.remove(this);
}

int ConnectionContext::get_buffer_timeout(MeshBuffer **buf, int timeout_ms)
{
    if (!buf)
        return -MESH_ERR_BAD_BUF_PTR;

    BufferContext *buf_ctx = new(std::nothrow) BufferContext(this);
    if (!buf_ctx)
        return -ENOMEM;

    if (timeout_ms == MESH_TIMEOUT_DEFAULT && __public.client)
        timeout_ms = ((ClientContext *)__public.client)->config.timeout_ms;

    int err = buf_ctx->dequeue(timeout_ms);
    if (err) {
        delete buf_ctx;
        return err;
    }

    *buf = (MeshBuffer *)buf_ctx;

    return 0;
}

} // namespace mesh
