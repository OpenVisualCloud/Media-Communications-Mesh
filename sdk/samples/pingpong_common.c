/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define _GNU_SOURCE
#include <bsd/string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pingpong_common.h"

int init_conn(MeshConnection *conn, Config *config, int kind, int id)
{
    int err;

    /* protocol type */
    if (!strcmp(config->protocol_type, "memif")) {
        MeshConfig_Memif cfg;

        strlcpy(cfg.socket_path, config->socket_path, sizeof(cfg.socket_path));
        cfg.interface_id = config->interface_id;

        err = mesh_apply_connection_config_memif(conn, &cfg);
        if (err) {
            printf("Failed to apply memif configuration: %s (%d)\n", mesh_err2str(err), err);
            return -1;
        }
    } else {
        if (!strcmp(config->payload_type, "rdma")) {
            MeshConfig_RDMA cfg;

            strlcpy(cfg.remote_ip_addr, config->send_addr, sizeof(cfg.remote_ip_addr));
            cfg.remote_port = atoi(config->send_port) + id;
            strlcpy(cfg.local_ip_addr, config->recv_addr, sizeof(cfg.local_ip_addr));
            cfg.local_port = atoi(config->recv_port) + id;

            err = mesh_apply_connection_config_rdma(conn, &cfg);
            if (err) {
                printf("Failed to apply RDMA configuration: %s (%d)\n", mesh_err2str(err), err);
                return -1;
            }
        } else {
            MeshConfig_ST2110 cfg;

            strlcpy(cfg.remote_ip_addr, config->send_addr, sizeof(cfg.remote_ip_addr));
            cfg.remote_port = atoi(config->send_port) + id;
            strlcpy(cfg.local_ip_addr, config->recv_addr, sizeof(cfg.local_ip_addr));
            cfg.local_port = atoi(config->recv_port) + id;

            /* transport type */
            if (!strcmp(config->payload_type, "st20")) {
                cfg.transport = MESH_CONN_TRANSPORT_ST2110_20;
            } else if (!strcmp(config->payload_type, "st22")) {
                cfg.transport = MESH_CONN_TRANSPORT_ST2110_22;
            } else if (!strcmp(config->payload_type, "st30")) {
                cfg.transport = MESH_CONN_TRANSPORT_ST2110_30;
            } else {
                printf("Unknown SMPTE ST2110 transport type: %s\n", config->payload_type);
                return -1;
            }

            err = mesh_apply_connection_config_st2110(conn, &cfg);
            if (err) {
                printf("Failed to apply SMPTE ST2110 configuration: %s (%d)\n", mesh_err2str(err),
                       err);
                return -1;
            }
        }
    }

    /* payload type */
    if (!strcmp(config->payload_type, "st20") || !strcmp(config->payload_type, "st22") ||
        !strcmp(config->payload_type, "rdma")) {
        /* video */
        MeshConfig_Video cfg;

        if (!strncmp(config->pix_fmt_string, "yuv422p10le", sizeof(config->pix_fmt_string)))
            cfg.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE;
        else if (!strncmp(config->pix_fmt_string, "v210", sizeof(config->pix_fmt_string)))
            cfg.pixel_format = MESH_VIDEO_PIXEL_FORMAT_V210;
        else if (!strncmp(config->pix_fmt_string, "yuv422p10rfc4175", sizeof(config->pix_fmt_string)))
            cfg.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10;
        else {
            printf("Unknown video pixel format: %s\n", config->pix_fmt_string);
            return -1;
        }

        cfg.width = config->width;
        cfg.height = config->height;
        cfg.fps = config->vid_fps;

        err = mesh_apply_connection_config_video(conn, &cfg);
        if (err) {
            printf("Failed to apply video configuration: %s (%d)\n", mesh_err2str(err), err);
            return -1;
        }
    } else if (!strcmp(config->payload_type, "st30")) {
        /* audio */
        MeshConfig_Audio cfg;

        cfg.channels = 2;
        cfg.format = MESH_AUDIO_FORMAT_PCM_S16BE;
        cfg.sample_rate = MESH_AUDIO_SAMPLE_RATE_48000;
        cfg.packet_time = MESH_AUDIO_PACKET_TIME_1MS;

        err = mesh_apply_connection_config_audio(conn, &cfg);
        if (err) {
            printf("Failed to apply audio configuration: %s (%d)\n", mesh_err2str(err), err);
            return -1;
        }
    } else {
        printf("Unknown payload type: %s\n", config->payload_type);
        return -1;
    }

    err = mesh_establish_connection(conn, kind);
    if (err) {
        printf("Failed to establish connection: %s (%d)\n", mesh_err2str(err), err);
        return -1;
    }

    return 0;
}
