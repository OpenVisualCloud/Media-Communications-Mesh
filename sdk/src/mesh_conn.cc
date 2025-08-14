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
#include <thread>
#include <stop_token>
#include "mesh_dp_legacy.h"

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct mesh_internal_ops_t mesh_internal_ops = {
    .create_client            = mesh::create_proxy_client,
    .destroy_client           = mesh::destroy_proxy_client,
    .create_conn              = mesh::create_proxy_conn,
    .destroy_conn             = mesh::destroy_proxy_conn,
    .create_conn_zero_copy    = mesh::create_proxy_conn_zero_copy,
    .configure_conn_zero_copy = mesh::configure_proxy_conn_zero_copy,
    .destroy_conn_zero_copy   = mesh::destroy_proxy_conn_zero_copy,
    .dequeue_buf              = mcm_dequeue_buffer,
    .enqueue_buf              = mcm_enqueue_buffer,
};

namespace mesh {

int ConnectionConfig::apply_json_config(const char *config) {

    auto err = parse_from_json(config);
    if (err)
        return err;

    log::debug("JSON conn config: %s", config);

    err = calc_payload_size();
    if (err)
        return err;

    return configure_buf_partitions();
}

int ConnectionConfig::parse_from_json(const char *str)
{
    try {
        auto j = nlohmann::json::parse(str);

        name = j.value("name", "");
        buf_queue_capacity = j.value("bufferQueueCapacity", 16);
        max_payload_size = j.value("maxPayloadSize", 0);
        max_metadata_size = j.value("maxMetadataSize", 0);

        tx_conn_creation_delay = j.value("connCreationDelayMilliseconds", 0);

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
            conn.st2110.ip_addr = st2110.value("ipAddr", "");
            conn.st2110.port = st2110.value("port", 0);
            conn.st2110.mcast_sip_addr = st2110.value("multicastSourceIpAddr", "");

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

        if (j.contains("options")) {
            auto joptions = j["options"];

            options.engine = joptions.value("engine", "");

            if (joptions.contains("rdma")) {
                auto rdma = joptions["rdma"];

                std::string str = rdma.value("provider", "tcp");
                if (!str.compare("tcp") || !str.compare("verbs")) {
                    options.rdma.provider = str;
                } else {
                    log::error("rdma: wrong provider: %s", str.c_str());
                    return -MESH_ERR_CONN_CONFIG_INVAL;
                }                

                uint32_t num_endpoints = rdma.value("num_endpoints", 1);
                if (num_endpoints >= 1 && num_endpoints <= 8) {
                    options.rdma.num_endpoints = num_endpoints;
                } else {
                    log::error("rdma: number of endpoints out of range (1..8): %u", num_endpoints);
                    return -MESH_ERR_CONN_CONFIG_INVAL;
                }                
            }
        }

        if (!j.contains("payload")) {
            payload_type = MESH_PAYLOAD_TYPE_BLOB;
        } else {
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
        }

        if (payload_type == MESH_PAYLOAD_TYPE_BLOB) {
            if (conn_type != MESH_CONN_TYPE_GROUP) {
                log::error("blob: conn type must be multipoint group");
                return -MESH_ERR_CONN_CONFIG_INCOMPAT;
            }
            if (!max_payload_size) {
                log::error("blob: non-zero max payload size must be specified");
                return -MESH_ERR_CONN_CONFIG_INVAL;
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

int ConnectionConfig::calc_audio_buf_size()
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

int ConnectionConfig::calc_video_buf_size()
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

int ConnectionConfig::calc_payload_size()
{
    switch (payload_type) {
    case MESH_PAYLOAD_TYPE_VIDEO:
        return calc_video_buf_size();
    case MESH_PAYLOAD_TYPE_AUDIO:
        return calc_audio_buf_size();
    case MESH_PAYLOAD_TYPE_BLOB:
        calculated_payload_size = max_payload_size;
        return 0;
    }

    return -MESH_ERR_CONN_CONFIG_INVAL;
}

int ConnectionConfig::configure_buf_partitions()
{
    buf_parts.sysdata.offset = 0;
    buf_parts.sysdata.size = (sizeof(BufferSysData) + 7) & ~7;

    buf_parts.payload.offset = buf_parts.sysdata.size;
    buf_parts.payload.size = (calculated_payload_size + 7) & ~7;

    buf_parts.metadata.offset = buf_parts.payload.offset + buf_parts.payload.size;
    buf_parts.metadata.size = (max_metadata_size + 7) & ~7;

    log::debug("BUF PARTS sysdata %u %u, payload %u %u, meta %u %u",
               buf_parts.sysdata.offset, buf_parts.sysdata.size,
               buf_parts.payload.offset, buf_parts.payload.size,
               buf_parts.metadata.offset, buf_parts.metadata.size);

    return 0;
}

int ConnectionConfig::assign_to_mcm_conn_param(mcm_conn_param& param) const
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

void ConnectionContext::assign_config(ConnectionConfig& cfg)
{
    cfg.kind = this->cfg.kind;
    this->cfg = std::move(cfg);
}

ConnectionContext::~ConnectionContext()
{
    ClientContext *mc_ctx = (ClientContext *)__public.client;

    std::lock_guard<std::mutex> lk(mc_ctx->mx);

    mc_ctx->conns.remove(this);
}

} // namespace mesh
