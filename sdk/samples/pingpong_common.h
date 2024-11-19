/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel CorporationPINGPONG_COMMON_H
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PINGPONG_COMMON_H
#define PINGPONG_COMMON_H

#include "mesh_dp.h"

#define DEFAULT_RECV_IP "127.0.0.1"
#define DEFAULT_SEND_IP "127.0.0.1"

#define DEFAULT_TOTAL_NUM 300
#define DEFAULT_FRAME_WIDTH 1920
#define DEFAULT_FRAME_HEIGHT 1080
#define DEFAULT_FPS 2.0
#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"
#define DEFAULT_MEMIF_INTERFACE_ID 0
#define DEFAULT_PROTOCOL "auto"
#define DEFAULT_INFINITE_LOOP 0
#define DEFAULT_VIDEO_FMT "yuv422p10le"

#define CPU_CORES 28     // Number of available CPU cores
#define TRANSFERS_NUM 16 // Number of buffers to sent

typedef struct {
    char recv_addr[46];
    char recv_port[6];
    char send_addr[46];
    char send_port[6];

    char payload_type[32];
    char protocol_type[32];
    char pix_fmt_string[32];
    char socket_path[108];
    uint32_t interface_id;
    bool loop;

    /* video resolution */
    uint32_t width;
    uint32_t height;
    double vid_fps;
    uint32_t frame_size;
    uint32_t total_num;

    int threads_num;
} Config;

int init_conn(MeshConnection *conn, Config *config, int kind, int id);

#endif /* PINGPONG_COMMON_H */
