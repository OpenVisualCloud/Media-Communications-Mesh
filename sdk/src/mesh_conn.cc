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
#include "mesh_sdk_api.h"
#include "mesh_logger.h"
#include "json.hpp"

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
    .grpc_create_conn_json = mesh_grpc_create_conn_json,
    .grpc_destroy_conn = mesh_grpc_destroy_conn,
};

namespace mesh {

int ConnectionJsonConfig::parse_json(const char *str)
{
    try {
        auto j = nlohmann::json::parse(str);

        buf_queue_capacity = j.value("bufferQueueCapacity", 16);
        max_payload_size = j.value("maxPayloadSize", 0);
        max_metadata_size = j.value("maxMetadataSize", 0);

        if (!j.contains("connection")) {
            log::error("connection config not specified");
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        auto jconn = j["connection"];

        if (jconn.contains("multipointGroup"))
            conn_type = MESH_CONN_TYPE_GROUP;

        if (jconn.contains("st2110")) {
            if (conn_type != MESH_CONN_TYPE_UNINITIALIZED) {
                log::error("connection.st2110 config err: multiple conn types")("conn_type", conn_type);
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }
            conn_type = MESH_CONN_TYPE_ST2110;
        }

        if (jconn.contains("rdma")) {
            if (conn_type != MESH_CONN_TYPE_UNINITIALIZED) {
                log::error("connection.rdma config err: multiple conn types");
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }
            conn_type = MESH_CONN_TYPE_RDMA;
        }

        if (conn_type == MESH_CONN_TYPE_UNINITIALIZED) {
            log::error("connection config type not specified");
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }
        if (conn_type == MESH_CONN_TYPE_GROUP) {
            auto group = jconn["multipointGroup"];
            conn.multipoint_group.urn = group.value("urn", "");
        } else if (conn_type == MESH_CONN_TYPE_ST2110) {
            auto st2110 = jconn["st2110"];
            conn.st2110.remote_ip_addr = st2110.value("remoteIpAddr", "");
            conn.st2110.remote_port = st2110.value("remotePort", 0);

            std::string str = st2110.value("transport", "st2110-20");
            if (!str.compare("st2110-20")) {
                conn.st2110.transport = MESH_CONN_TRANSPORT_ST2110_20;
            } else if (!str.compare("st2110-22")) {
                conn.st2110.transport = MESH_CONN_TRANSPORT_ST2110_22;
            } else if (!str.compare("st2110-30")) {
                conn.st2110.transport = MESH_CONN_TRANSPORT_ST2110_30;
            } else {
                log::error("st2110: wrong transport: %s", str.c_str());
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }

            conn.st2110.pacing = st2110.value("pacing", "");
            conn.st2110.payload_type = st2110.value("payloadType", 112);
            if (conn.st2110.transport == MESH_CONN_TRANSPORT_ST2110_20) {
                conn.st2110.transportPixelFormat =
                    st2110.value("transportPixelFormat", "yuv422p10rfc4175");
            }
        } else if (conn_type == MESH_CONN_TYPE_RDMA) {
            auto rdma = jconn["rdma"];
            conn.rdma.connection_mode = rdma.value("connectionMode", "RC");
            conn.rdma.max_latency_ns = rdma.value("maxLatencyNanoseconds", 0);
        }

        if (!j.contains("payload")) {
            payload_type = MESH_PAYLOAD_TYPE_BLOB;
            return 0;
        }

        auto jpayload = j["payload"];

        if (jpayload.contains("video"))
            payload_type = MESH_PAYLOAD_TYPE_VIDEO;

        if (jpayload.contains("audio")) {
            if (payload_type != MESH_PAYLOAD_TYPE_UNINITIALIZED) {
                log::error("payload.audio config err: multiple payload types");
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }
            payload_type = MESH_PAYLOAD_TYPE_AUDIO;
        }

        if (jpayload.contains("blob")) {
            if (payload_type != MESH_PAYLOAD_TYPE_UNINITIALIZED) {
                log::error("payload.blob config err: multiple payload types");
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }
            payload_type = MESH_PAYLOAD_TYPE_BLOB;
        }

        if (payload_type == MESH_PAYLOAD_TYPE_UNINITIALIZED) {
            log::error("payload config type not specified");
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        if (payload_type == MESH_PAYLOAD_TYPE_VIDEO) {
            auto video = jpayload["video"];
            payload.video.width = video.value("width", 640);
            payload.video.height = video.value("height", 640);
            payload.video.fps = video.value("fps", 60.0);

            std::string str = video.value("pixelFormat", "yuv422p10le");
            if (!str.compare("yuv422p10le")) {
                payload.video.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE;
            } else if (!str.compare("v210")) {
                payload.video.pixel_format = MESH_VIDEO_PIXEL_FORMAT_V210;
            } else if (!str.compare("yuv422p10rfc4175")) {
                payload.video.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10;
            } else {
                log::error("video: wrong pixel format: %s", str.c_str());
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }
        } else if (payload_type == MESH_PAYLOAD_TYPE_AUDIO) {
            auto audio = jpayload["audio"];
            payload.audio.channels = audio.value("channels", 2);

            std::string str = audio.value("format", "pcm_s24be");
            if (!str.compare("pcm_s24be")) {
                payload.audio.format = MESH_AUDIO_FORMAT_PCM_S24BE;
            } else if (!str.compare("pcm_s16be")) {
                payload.audio.format = MESH_AUDIO_FORMAT_PCM_S16BE;
            } else if (!str.compare("pcm_s8")) {
                payload.audio.format = MESH_AUDIO_FORMAT_PCM_S8;
            } else {
                log::error("audio: wrong format: %s", str.c_str());
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }

            int sample_rate = audio.value("sampleRate", 48000);
            switch (sample_rate) {
            case 44100:
                payload.audio.sample_rate = MESH_AUDIO_SAMPLE_RATE_44100;
                break;
            case 48000:
                payload.audio.sample_rate = MESH_AUDIO_SAMPLE_RATE_48000;
                break;
            case 96000:
                payload.audio.sample_rate = MESH_AUDIO_SAMPLE_RATE_96000;
                break;
            default:
                log::error("audio: wrong sample rate: %d", sample_rate);
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }

            str = audio.value("packetTime", "1ms");
            if (!str.compare("1ms")) {
            } else if (!str.compare("1ms")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_1MS;
            } else if (!str.compare("125us")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_125US;
            } else if (!str.compare("250us")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_250US;
            } else if (!str.compare("333us")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_333US;
            } else if (!str.compare("4ms")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_4MS;
            } else if (!str.compare("80us")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_80US;
            } else if (!str.compare("1.09ms")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_1_09MS;
            } else if (!str.compare("0.14ms")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_0_14MS;
            } else if (!str.compare("0.09ms")) {
                payload.audio.packet_time = MESH_AUDIO_PACKET_TIME_1_09MS;
            } else {
                log::error("audio: wrong packet time: %s", str.c_str());
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }

            bool compatible = false;
            switch (payload.audio.sample_rate) {
            case MESH_AUDIO_SAMPLE_RATE_48000:
            case MESH_AUDIO_SAMPLE_RATE_96000:
                switch (payload.audio.packet_time) {
                case MESH_AUDIO_PACKET_TIME_1MS:
                case MESH_AUDIO_PACKET_TIME_125US:
                case MESH_AUDIO_PACKET_TIME_250US:
                case MESH_AUDIO_PACKET_TIME_333US:
                case MESH_AUDIO_PACKET_TIME_4MS:
                case MESH_AUDIO_PACKET_TIME_80US:
                    compatible = true;
                    break;
                }
                break;
            case MESH_AUDIO_SAMPLE_RATE_44100:
                switch (payload.audio.packet_time) {
                case MESH_AUDIO_PACKET_TIME_1_09MS:
                case MESH_AUDIO_PACKET_TIME_0_14MS:
                case MESH_AUDIO_PACKET_TIME_0_09MS:
                    compatible = true;
                    break;
                }
                break;
            }
            if (!compatible) {
                log::error("audio: sample rate incompatible with packet time");
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;
            }
        }

        return 0;
    } catch (const nlohmann::json::exception& e) {
        log::error("conn cfg json parse err: %s", e.what());
    } catch (const std::exception& e) {
        log::error("conn cfg json parse std err: %s", e.what());
    }
    return -MESH_ERR_CONN_CONFIG_INVAL;
}

int ConnectionJsonConfig::calc_audio_buf_size()
{
    uint32_t sample_size = 0;
    uint32_t sample_num = 0;
    calculated_payload_size = 0;

    switch (payload.audio.format) {
    case MESH_AUDIO_FORMAT_PCM_S8:
        sample_size = 1;
        break;
    case MESH_AUDIO_FORMAT_PCM_S16BE:
        sample_size = 2;
        break;
    case MESH_AUDIO_FORMAT_PCM_S24BE:
        sample_size = 3;
        break;
    default:
        return -MESH_ERR_CONN_CONFIG_INVAL;
    }

    switch (payload.audio.sample_rate) {
    case MESH_AUDIO_SAMPLE_RATE_48000:
        switch (payload.audio.packet_time) {
        case MESH_AUDIO_PACKET_TIME_1MS:
            sample_num = 48;
            break;
        case MESH_AUDIO_PACKET_TIME_125US:
            sample_num = 6;
            break;
        case MESH_AUDIO_PACKET_TIME_250US:
            sample_num = 12;
            break;
        case MESH_AUDIO_PACKET_TIME_333US:
            sample_num = 16;
            break;
        case MESH_AUDIO_PACKET_TIME_4MS:
            sample_num = 192;
            break;
        case MESH_AUDIO_PACKET_TIME_80US:
            sample_num = 4;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }
        break;
    case MESH_AUDIO_SAMPLE_RATE_96000:
        switch (payload.audio.packet_time) {
        case MESH_AUDIO_PACKET_TIME_1MS:
            sample_num = 96;
            break;
        case MESH_AUDIO_PACKET_TIME_125US:
            sample_num = 12;
            break;
        case MESH_AUDIO_PACKET_TIME_250US:
            sample_num = 24;
            break;
        case MESH_AUDIO_PACKET_TIME_333US:
            sample_num = 32;
            break;
        case MESH_AUDIO_PACKET_TIME_4MS:
            sample_num = 384;
            break;
        case MESH_AUDIO_PACKET_TIME_80US:
            sample_num = 8;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }
        break;
    case MESH_AUDIO_SAMPLE_RATE_44100:
        switch (payload.audio.packet_time) {
        case MESH_AUDIO_PACKET_TIME_1_09MS:
            sample_num = 48;
            break;
        case MESH_AUDIO_PACKET_TIME_0_14MS:
            sample_num = 6;
            break;
        case MESH_AUDIO_PACKET_TIME_0_09MS:
            sample_num = 4;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }
        break;
    default:
        return -EINVAL;
    }

    calculated_payload_size = sample_size * sample_num * payload.audio.channels;

    return 0;
}

int ConnectionJsonConfig::calc_video_buf_size()
{
    uint32_t pixels = payload.video.width * payload.video.height;

    switch (payload.video.pixel_format) {
        case MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE:
            calculated_payload_size = pixels * 4;
            break;

        case MESH_VIDEO_PIXEL_FORMAT_V210:
            if (pixels % 3) {
                log::error("Invalid width %u height %u for v210 fmt, not multiple of 3",
                           payload.video.width, payload.video.height);
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }
            calculated_payload_size = pixels * 8 / 3;
            break;

        case MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10:
            if (pixels % 2) {
                log::error("Invalid width %u height %u for yuv422rfc4175be10 fmt, not multiple of 2",
                           payload.video.width, payload.video.height);
                return -MESH_ERR_CONN_CONFIG_INVAL;
            }
            calculated_payload_size = pixels * 5 / 2;
            break;

        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
    }
    
    return 0;
}

int ConnectionJsonConfig::calc_payload_size()
{
    switch (payload_type) {
    case MESH_PAYLOAD_TYPE_VIDEO:
        return calc_video_buf_size();
    case MESH_PAYLOAD_TYPE_AUDIO:
        return calc_audio_buf_size();
    }

    return -MESH_ERR_CONN_CONFIG_INVAL;
}

int ConnectionJsonConfig::assign_to_mcm_conn_param(mcm_conn_param& param) const
{
    /**
     * Parse video payload parameters
     */
    if (payload_type == MESH_PAYLOAD_TYPE_VIDEO) {

        switch (payload.video.pixel_format) {
        case MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE:
            param.pix_fmt = PIX_FMT_YUV422PLANAR10LE;
            break;            
        case MESH_VIDEO_PIXEL_FORMAT_V210:
            param.pix_fmt = PIX_FMT_V210;
            break;            
        case MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10:
            param.pix_fmt = PIX_FMT_YUV422RFC4175BE10;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        param.payload_args.video_args.pix_fmt = param.pix_fmt;
        param.payload_args.video_args.width   = payload.video.width;
        param.width                           = payload.video.width;
        param.payload_args.video_args.height  = payload.video.height;
        param.height                          = payload.video.height;
        param.payload_args.video_args.fps     = param.fps = payload.video.fps;

        return 0;
    }

    /**
     * Parse audio payload parameters
     */
    if (payload_type == MESH_PAYLOAD_TYPE_AUDIO) {

        switch (payload.audio.sample_rate) {
        case MESH_AUDIO_SAMPLE_RATE_44100:
            param.payload_args.audio_args.sampling = AUDIO_SAMPLING_44K;
            break; 
        case MESH_AUDIO_SAMPLE_RATE_48000:
            param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
            break;
        case MESH_AUDIO_SAMPLE_RATE_96000:
            param.payload_args.audio_args.sampling = AUDIO_SAMPLING_96K;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        switch (payload.audio.sample_rate) {
        case MESH_AUDIO_SAMPLE_RATE_48000:
        case MESH_AUDIO_SAMPLE_RATE_96000:
            switch (payload.audio.packet_time) {
            case MESH_AUDIO_PACKET_TIME_1MS:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;
                break;
            case MESH_AUDIO_PACKET_TIME_125US:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_125US;
                break;
            case MESH_AUDIO_PACKET_TIME_250US:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_250US;
                break;
            case MESH_AUDIO_PACKET_TIME_333US:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_333US;
                break;
            case MESH_AUDIO_PACKET_TIME_4MS:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_4MS;
                break;
            case MESH_AUDIO_PACKET_TIME_80US:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_80US;
                break;
            default:
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;
            }
            break;
        case MESH_AUDIO_SAMPLE_RATE_44100:
            switch (payload.audio.packet_time) {
            case MESH_AUDIO_PACKET_TIME_1_09MS:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_1_09MS;
                break;
            case MESH_AUDIO_PACKET_TIME_0_14MS:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_0_14MS;
                break;
            case MESH_AUDIO_PACKET_TIME_0_09MS:
                param.payload_args.audio_args.ptime = AUDIO_PTIME_0_09MS;
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

        switch (payload.audio.format) {
        case MESH_AUDIO_FORMAT_PCM_S8:
            param.payload_args.audio_args.format = AUDIO_FMT_PCM8;
            break;
        case MESH_AUDIO_FORMAT_PCM_S16BE:
            param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
            break;
        case MESH_AUDIO_FORMAT_PCM_S24BE:
            param.payload_args.audio_args.format = AUDIO_FMT_PCM24;
            break;
        default:
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
        param.payload_args.audio_args.channel = payload.audio.channels;

        return 0;
    }

    return -MESH_ERR_CONN_CONFIG_INVAL;
}

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
        case MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE:
            param->pix_fmt = PIX_FMT_YUV422PLANAR10LE;
            break;            
        case MESH_VIDEO_PIXEL_FORMAT_V210:
            param->pix_fmt = PIX_FMT_V210;
            break;            
        case MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10:
            param->pix_fmt = PIX_FMT_YUV422RFC4175BE10;
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

int ConnectionContext::apply_json_config(const char *config) {

    auto err = cfg_json.parse_json(config);
    if (err)
        return err;

    log::debug("JSON conn config: %s", config);

    return cfg_json.calc_payload_size();
}

int ConnectionContext::establish_json()
{
    int err;

    if (handle)
        return -MESH_ERR_BAD_CONN_PTR;

    ClientContext *mc_ctx = (ClientContext *)__public.client;
    if (!mc_ctx)
        return -MESH_ERR_BAD_CLIENT_PTR;

    grpc_conn = mesh_internal_ops.grpc_create_conn_json(mc_ctx->grpc_client,
                                                        cfg_json);
    if (!grpc_conn) {
        handle = NULL;
        return -MESH_ERR_CONN_FAILED;
    }
    handle = *(mcm_conn_context **)grpc_conn; // unsafe type casting
    if (!handle)
        return -MESH_ERR_CONN_FAILED;

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
