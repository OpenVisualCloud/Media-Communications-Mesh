/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pthread.h>
#include "mcm_common.h"
#include <bsd/string.h>
#include <stdatomic.h>
#include "libavutil/pixdesc.h"

static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static MeshClient *client;
static int refcnt;

void mcm_replace_back_quotes(char *str) {
    while (*str) {
        if (*str == '`')
            *str = '"';
        str++;
    }
}

/**
 * Get MCM client. Create one if needed.
 * Thread-safe.
 */
int mcm_get_client(MeshClient **mc)
{
    int err;

    err = pthread_mutex_lock(&mx);
    if (err)
        return err;

    if (!client) {
        static char json_config[150];

        static const char json_config_format[] =
            "{"
            "`apiVersion`: `v1`,"
            "`apiConnectionString`: `Server=; Port=`,"
            "`apiDefaultTimeoutMicroseconds`: 100000,"
            "`maxMediaConnections`: 32"
            "}";

        snprintf(json_config, sizeof(json_config), json_config_format);
        mcm_replace_back_quotes(json_config);

        err = mesh_create_client_json(&client, json_config);
        if (err)
            client = NULL;
        else
            refcnt = 1;
    } else {
        refcnt++;
    }

    pthread_mutex_unlock(&mx);

    *mc = client;

    return err;
}

/**
 * Put MCM client, or release the client.
 * Thread-safe.
 */
int mcm_put_client(MeshClient **mc)
{
    int err;

    err = pthread_mutex_lock(&mx);
    if (err)
        return err;

    if (!client) {
        err = -EINVAL;
    } else {
        if (--refcnt == 0)
            mesh_delete_client(&client);

        *mc = NULL;
    }

    pthread_mutex_unlock(&mx);

    return err;
}

const char mcm_json_config_multipoint_group_video_format[] =
    "{"
      "`connection`: {"
        "`multipointGroup`: {"
          "`urn`: `%s`"
        "}"
      "},"
      "`payload`: {"
        "`video`: {"
          "`width`: %d,"
          "`height`: %d,"
          "`fps`: %0.2f,"
          "`pixelFormat`: `%s`"
        "}"
      "}"
    "}";

const char mcm_json_config_st2110_video_format[] =
    "{"
      "`connection`: {"
        "`st2110`: {"
          "`remoteIpAddr`: `%s`,"
          "`remotePort`: %d,"
          "`transport`: `%s`,"
          "`transportPixelFormat`: `%s`"
        "}"
      "},"
      "`payload`: {"
        "`video`: {"
          "`width`: %d,"
          "`height`: %d,"
          "`fps`: %0.2f,"
          "`pixelFormat`: `%s`"
        "}"
      "}"
    "}";

const char mcm_json_config_multipoint_group_audio_format[] =
    "{"
      "`connection`: {"
        "`multipointGroup`: {"
          "`urn`: `%s`"
        "}"
      "},"
      "`payload`: {"
        "`audio`: {"
          "`channels`: %d,"
          "`sampleRate`: %d,"
          "`format`: `%s`,"
          "`packetTime`: `%s`"
        "}"
      "}"
    "}";

const char mcm_json_config_st2110_audio_format[] =
    "{"
      "`connection`: {"
        "`st2110`: {"
          "`remoteIpAddr`: `%s`,"
          "`remotePort`: %d,"
          "`transport`: `st2110-30`"
        "}"
      "},"
      "`payload`: {"
        "`audio`: {"
          "`channels`: %d,"
          "`sampleRate`: %d,"
          "`format`: `%s`,"
          "`packetTime`: `%s`"
        "}"
      "}"
    "}";

/**
 * Parse MCM connection parameters and fill the structure.
 */
int mcm_parse_conn_param(AVFormatContext* avctx, MeshConnection *conn,
                         int kind, char *ip_addr, char *port,
                         char *protocol_type, char *payload_type,
                         char *socket_name, int interface_id)
{
    int err;

    /* protocol type */
    if (!strcmp(protocol_type, "memif")) {
        MeshConfig_Memif cfg;

        snprintf(cfg.socket_path, sizeof(cfg.socket_path),
            "/run/mcm/mcm_memif_%s.sock", socket_name ? socket_name : "0");
        cfg.interface_id = interface_id;

        err = mesh_apply_connection_config_memif(conn, &cfg);
        if (err)
            return err;
    } else if (!strcmp(payload_type, "rdma")) {
        MeshConfig_RDMA cfg;

        if (kind == MESH_CONN_KIND_SENDER) {
            cfg.remote_port = atoi(port);
            strlcpy(cfg.remote_ip_addr, ip_addr, sizeof(cfg.remote_ip_addr));
        } else {
            cfg.local_port = atoi(port);
            strlcpy(cfg.local_ip_addr, ip_addr, sizeof(cfg.local_ip_addr));
        }

        err = mesh_apply_connection_config_rdma(conn, &cfg);
        if (err)
           return err;
    } else {
        MeshConfig_ST2110 cfg;

        strlcpy(cfg.remote_ip_addr, ip_addr, sizeof(cfg.remote_ip_addr));
        
        if (kind == MESH_CONN_KIND_SENDER)
            cfg.remote_port = atoi(port);
        else
            cfg.local_port = atoi(port);

        /* transport type */
        if (!strcmp(payload_type, "st20")) {
            cfg.transport = MESH_CONN_TRANSPORT_ST2110_20;
        } else if (!strcmp(payload_type, "st22")) {
            cfg.transport = MESH_CONN_TRANSPORT_ST2110_22;
        } else if (!strcmp(payload_type, "st30")) {
            cfg.transport = MESH_CONN_TRANSPORT_ST2110_30;
        } else {
            av_log(avctx, AV_LOG_ERROR, "Unknown payload type\n");
            return -MESH_ERR_CONN_CONFIG_INVAL;
        }

        err = mesh_apply_connection_config_st2110(conn, &cfg);
        if (err)
            return err;
    }

    return 0;
}

/**
 * Parse MCM video pixel format
 */
int mcm_parse_video_pix_fmt(AVFormatContext* avctx, int *pix_fmt,
                            enum AVPixelFormat value)
{
    switch (value) {
    case AV_PIX_FMT_YUV422P10LE:
        *pix_fmt = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE;
        return 0;
    default:
        av_log(avctx, AV_LOG_ERROR, "Unknown pixel format (%s)\n",
               av_get_pix_fmt_name(value));
        return AVERROR(EINVAL);
    }
}

/**
 * Parse MCM audio sampling rate
 */
int mcm_parse_audio_sample_rate(AVFormatContext* avctx, int *sample_rate,
                                int value)
{
    switch (value) {
    case 44100:
        *sample_rate = MESH_AUDIO_SAMPLE_RATE_44100;
        return 0;
    case 48000:
        *sample_rate = MESH_AUDIO_SAMPLE_RATE_48000;
        return 0;
    case 96000:
        *sample_rate = MESH_AUDIO_SAMPLE_RATE_96000;
        return 0;
    default:
        av_log(avctx, AV_LOG_ERROR, "Audio sample rate not supported\n");
        return AVERROR(EINVAL);
    }
}

/**
 * Parse MCM audio packet time
 */
int mcm_parse_audio_packet_time(AVFormatContext* avctx, int *ptime, char *str)
{
    if (!str || !strcmp(str, "1ms")) {
        *ptime = MESH_AUDIO_PACKET_TIME_1MS;
        return 0;
    }
    if (!strcmp(str, "125us")) {
        *ptime = MESH_AUDIO_PACKET_TIME_125US;
        return 0;
    }
    if (!strcmp(str, "250us")) {
        *ptime = MESH_AUDIO_PACKET_TIME_250US;
        return 0;
    }
    if (!strcmp(str, "333us")) {
        *ptime = MESH_AUDIO_PACKET_TIME_333US;
        return 0;
    }
    if (!strcmp(str, "4ms")) {
        *ptime = MESH_AUDIO_PACKET_TIME_4MS;
        return 0;
    }
    if (!strcmp(str, "80us")) {
        *ptime = MESH_AUDIO_PACKET_TIME_80US;
        return 0;
    }
    if (!strcmp(str, "1.09ms")) {
        *ptime = MESH_AUDIO_PACKET_TIME_1_09MS;
        return 0;
    }
    if (!strcmp(str, "0.14ms")) {
        *ptime = MESH_AUDIO_PACKET_TIME_0_14MS;
        return 0;
    }
    if (!strcmp(str, "0.09ms")) {
        *ptime = MESH_AUDIO_PACKET_TIME_0_09MS;
        return 0;
    }

    av_log(avctx, AV_LOG_ERROR, "Audio packet time not supported\n");
    return AVERROR(EINVAL);
}
