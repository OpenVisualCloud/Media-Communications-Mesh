/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsd/string.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <mcm_dp.h>

#define DEFAULT_RECV_IP "127.0.0.1"
#define DEFAULT_RECV_PORT "9001"

static volatile bool keepRunning = true;

void intHandler(int dummy)
{
    keepRunning = 0;
}

/* print a description of all supported options */
void usage(FILE* fp, const char* path)
{
    /* take only the last portion of the path */
    const char* basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(fp, "usage: %s [OPTION]\n", basename);
    fprintf(fp, "-h, --help\t\t\t"
                "Print this help and exit.\n");
    fprintf(fp, "-r, --ip=ip_address\t\t"
                "Receive data from IP address (defaults: %s).\n", DEFAULT_RECV_IP);
    fprintf(fp, "-p, --port=port_number\t"
                "Receive data from Port (defaults: %s).\n", DEFAULT_RECV_PORT);
}

int main(int argc, char** argv)
{
    char recv_addr[46] = DEFAULT_RECV_IP;
    char recv_port[6] = DEFAULT_RECV_PORT;

    mcm_conn_context* dp_ctx = NULL;
    mcm_conn_param param = { 0 };
    mcm_buffer* buf = NULL;

    int help_flag = 0;
    int opt;
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 1 },
        { "ip", required_argument, NULL, 'r' },
        { "port", required_argument, NULL, 'p' },
        { 0 }
    };

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "Hr:p:", longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'H':
            help_flag = 1;
            break;
        case 'r':
            strlcpy(recv_addr, optarg, sizeof(recv_addr));
            break;
        case 'p':
            strlcpy(recv_port, optarg, sizeof(recv_port));
            break;
        case '?':
            usage(stderr, argv[0]);
            return 1;
        default:
            break;
        }
    }

    if (help_flag) {
        usage (stdout, argv[0]);
        return 0;
    }

    /* is receiver */
    param.type = is_rx;
    param.payload_type = PAYLOAD_TYPE_RTSP_VIDEO;

    strlcpy(param.remote_addr.ip, recv_addr, sizeof(param.remote_addr.ip));
    strlcpy(param.local_addr.port, recv_port, sizeof(param.local_addr.port));

    dp_ctx = mcm_create_connection(&param);
    if (dp_ctx == NULL) {
        printf("Fail to connect to MCM data plane.\n");
        exit(-1);
    }

    signal(SIGINT, intHandler);

    uint32_t frame_count = 0;
    const uint32_t fps_interval = 30;
    double fps = 0.0;
    void* ptr = NULL;
    int timeout = -1;
    bool first_frame = true;
    long latency = 0;
    struct timespec ts_recv = {}, ts_send = {};
    struct timespec ts_begin = {}, ts_end = {};

    clock_gettime(CLOCK_REALTIME, &ts_begin);
    while (keepRunning) {
        /* receive frame */
        if (first_frame) {
            /* infinity for the 1st frame. */
            timeout = -1;
        } else {
            /* 1 second */
            timeout = 1000;
        }
        buf = mcm_dequeue_buffer(dp_ctx, timeout, NULL);
        if (buf == NULL) {
            break;
        }

        clock_gettime(CLOCK_REALTIME, &ts_recv);
        if (first_frame) {
            ts_begin = ts_recv;
            first_frame = false;
        }

        /* operate on the buffer */
        ptr = buf->data;
        if (*(uint32_t*)ptr != frame_count) {
            printf("Wrong data content: expected %d, got %u\n", frame_count, *(uint32_t*)ptr);
            /* catch up the sender frame count */
            frame_count = *(uint32_t*)ptr;
        }
        ptr += sizeof(frame_count);
        ts_send = *(struct timespec*)ptr;

        /* free buffer */
        if (mcm_enqueue_buffer(dp_ctx, buf) != 0) {
            break;
        }

        frame_count++;

        if (frame_count % fps_interval == 0) {
            /* calculate FPS */
            clock_gettime(CLOCK_REALTIME, &ts_end);

            fps = 1e9 * (ts_end.tv_sec - ts_begin.tv_sec);
            fps += (ts_end.tv_nsec - ts_begin.tv_nsec);
            fps /= 1e9;
            fps = (double)fps_interval / fps;

            clock_gettime(CLOCK_REALTIME, &ts_begin);
        }

        latency = 1000 * (ts_recv.tv_sec - ts_send.tv_sec);
        latency += (ts_recv.tv_nsec - ts_send.tv_nsec) / 1000000;

        printf("RX frames: [%u], latency: %ld ms, FPS: %0.3f\n",
            frame_count, latency, fps);
    }

    /* Clean up */
    printf("Destroy MCM connection.\n");
    mcm_destroy_connection(dp_ctx);

    return 0;
}
