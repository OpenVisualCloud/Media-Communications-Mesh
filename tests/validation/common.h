/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __COMMON_VAL_H
#define __COMMON_VAL_H

#include <assert.h>
#include <bsd/string.h>
#include <getopt.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "mesh_dp.h"
#include "mcm_dp.h"

#define DEFAULT_RECV_IP "127.0.0.1"
#define DEFAULT_RECV_PORT "9001"
#define DEFAULT_SEND_IP "127.0.0.1"
#define DEFAULT_SEND_PORT "9001"
#define DEFAULT_FRAME_WIDTH 1920
#define DEFAULT_FRAME_HEIGHT 1080
#define DEFAULT_FPS 30.0
#define DEFAULT_PAYLOAD_TYPE "st20"
#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"
#define DEFAULT_MEMIF_INTERFACE_ID 0
#define DEFAULT_PROTOCOL "auto"
#define DEFAULT_VIDEO_FMT "yuv422p10le"
#define DEFAULT_TOTAL_NUM 300
#define DEFAULT_INFINITE_LOOP 0
#define EXAMPLE_LOCAL_FILE "sample_video.yuv"

void usage(FILE* fp, const char* path, int sender);
void set_video_pix_fmt(int* pix_fmt, char* pix_fmt_string);

#endif /* __COMMON_VAL_H */
