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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mcm_dp.h"

#define DEFAULT_RECV_IP "127.0.0.1"
#define DEFAULT_RECV_PORT "9001"
#define DEFAULT_FRAME_WIDTH 1920
#define DEFAULT_FRAME_HEIGHT 1080
#define DEFAULT_FPS 30.0
#define DEFAULT_PAYLOAD_TYPE "st20"
#define DEFAULT_LOCAL_FILE "data-sdk.264"
#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"
#define DEFAULT_MEMIF_IS_MASTER 0
#define DEFAULT_MEMIF_INTERNFACE_ID 0
#define DEFAULT_PROTOCOL "auto"

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

    fprintf(fp, "Usage: %s [OPTION]\n", basename);
    fprintf(fp, "-H, --help\t\t\t"
                "Print this help and exit\n");
    fprintf(fp, "-w, --width=<frame_width>\t"
                "Width of test video frame (default: %d)\n",
        DEFAULT_FRAME_WIDTH);
    fprintf(fp, "-h, --height=<frame_height>\t"
                "Height of test video frame (default: %d)\n",
        DEFAULT_FRAME_HEIGHT);
    fprintf(fp, "-f, --fps=<video_fps>\t\t"
                "Test video FPS (frame per second) (default: %0.2f)\n",
        DEFAULT_FPS);
    fprintf(fp, "-r, --ip=ip_address\t\t"
                "Receive data from IP address (default: %s)\n",
        DEFAULT_RECV_IP);
    fprintf(fp, "-p, --port=port_number\t\t"
                "Receive data from Port (default: %s)\n",
        DEFAULT_RECV_PORT);
    fprintf(fp, "-o, --protocol=protocol_type\t"
                "Set protocol type (default: %s)\n",
        DEFAULT_PROTOCOL);
    fprintf(fp, "-t, --type=payload_type\t\t"
                "Payload type (default: %s)\n",
        DEFAULT_PAYLOAD_TYPE);
    fprintf(fp, "-s, --dumpfile=file_name\t"
                "Save stream to local file (example: %s)\n",
        DEFAULT_LOCAL_FILE);
    fprintf(fp, "-k, --socketpath=socket_path\t"
                "Set memif socket path (default: %s)\n",
        DEFAULT_MEMIF_SOCKET_PATH);
    fprintf(fp, "-m, --master=is_master\t\t"
                "Set memif conn is master (default: %d)\n",
        DEFAULT_MEMIF_IS_MASTER);
    fprintf(fp, "-d, --interfaceid=interface_id\t"
                "Set memif conn interface id (default: %d)\n",
        DEFAULT_MEMIF_INTERNFACE_ID);
    fprintf(fp, "\n");
}

int main(int argc, char** argv)
{
    int err = 0;
    char recv_addr[46] = DEFAULT_RECV_IP;
    char recv_port[6] = DEFAULT_RECV_PORT;
    char protocol_type[32] = DEFAULT_PROTOCOL;
    char payload_type[32] = DEFAULT_PAYLOAD_TYPE;
    char file_name[128] = "";
    char socket_path[108] = DEFAULT_MEMIF_SOCKET_PATH;
    uint8_t is_master = DEFAULT_MEMIF_IS_MASTER;
    uint32_t interface_id = DEFAULT_MEMIF_INTERNFACE_ID;

    /* video resolution */
    uint32_t width = DEFAULT_FRAME_WIDTH;
    uint32_t height = DEFAULT_FRAME_HEIGHT;
    double vid_fps = DEFAULT_FPS;
    video_pixel_format pix_fmt = PIX_FMT_NV12;

    mcm_conn_context* dp_ctx = NULL;
    mcm_conn_param param = {};
    mcm_buffer* buf = NULL;

    int help_flag = 0;
    int opt;
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 'H' },
        { "width", required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "fps", required_argument, NULL, 'f' },
        { "ip", required_argument, NULL, 'r' },
        { "port", required_argument, NULL, 'p' },
        { "protocol", required_argument, NULL, 'o' },
        { "type", required_argument, NULL, 't' },
        { "socketpath", required_argument, NULL, 'k' },
        { "master", required_argument, NULL, 'm' },
        { "interfaceid", required_argument, NULL, 'd' },
        { "dumpfile", required_argument, NULL, 's' },
        { 0 }
    };

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "Hw:h:f:r:p:o:t:s:k:m:d:", longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'H':
            help_flag = 1;
            break;
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case 'f':
            vid_fps = atof(optarg);
            break;
        case 'r':
            strlcpy(recv_addr, optarg, sizeof(recv_addr));
            break;
        case 'p':
            strlcpy(recv_port, optarg, sizeof(recv_port));
            break;
        case 'o':
            strlcpy(protocol_type, optarg, sizeof(protocol_type));
            break;
        case 't':
            strlcpy(payload_type, optarg, sizeof(payload_type));
            break;
        case 's':
            strlcpy(file_name, optarg, sizeof(file_name));
            break;
        case 'k':
            strlcpy(socket_path, optarg, sizeof(socket_path));
            break;
        case 'm':
            is_master = atoi(optarg);
            break;
        case 'd':
            interface_id = atoi(optarg);
            break;
        case '?':
            usage(stderr, argv[0]);
            return 1;
        default:
            break;
        }
    }

    if (help_flag) {
        usage(stdout, argv[0]);
        return 0;
    }

    /* is receiver */
    param.type = is_rx;

    /* protocol type */

    if (strncmp(protocol_type, "memif", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_MEMIF;
        strlcpy(param.memif_interface.socket_path, socket_path, sizeof(param.memif_interface.socket_path));
        param.memif_interface.is_master = is_master;
        param.memif_interface.interface_id = interface_id;
    } else if (strncmp(protocol_type, "udp", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_UDP;
    } else if (strncmp(protocol_type, "tcp", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_TCP;
    } else if (strncmp(protocol_type, "http", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_HTTP;
    } else if (strncmp(protocol_type, "grpc", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_GRPC;
    } else {
        param.protocol = PROTO_AUTO;
    }

    /* payload type */
    if (strncmp(payload_type, "st20", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_ST20_VIDEO;
    } else if (strncmp(payload_type, "st22", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_ST22_VIDEO;
    } else if (strncmp(payload_type, "st30", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_ST30_AUDIO;
    } else if (strncmp(payload_type, "st40", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_ST40_ANCILLARY;
    } else if (strncmp(payload_type, "rtsp", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_RTSP_VIDEO;
    } else {
        param.payload_type = PAYLOAD_TYPE_NONE;
    }

    switch (param.payload_type) {
    case PAYLOAD_TYPE_ST30_AUDIO:
        /* audio format */
        param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
        param.payload_args.audio_args.channel = 2;
        param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
        param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
        param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;
        break;
    case PAYLOAD_TYPE_ST40_ANCILLARY:
        /* ancillary format */
        param.payload_args.anc_args.format = ANC_FORMAT_CLOSED_CAPTION;
        param.payload_args.anc_args.type = ANC_TYPE_FRAME_LEVEL;
        param.payload_args.anc_args.fps = vid_fps;
        break;
    case PAYLOAD_TYPE_RTSP_VIDEO:
    case PAYLOAD_TYPE_ST20_VIDEO:
    case PAYLOAD_TYPE_ST22_VIDEO:
    default:
        /* video format */
        param.pix_fmt = PIX_FMT_YUV444M;
        param.payload_args.video_args.width   = param.width = width;
        param.payload_args.video_args.height  = param.height = height;
        param.payload_args.video_args.fps     = param.fps = vid_fps;
        param.payload_args.video_args.pix_fmt = param.pix_fmt = pix_fmt;
        break;
    }

    strlcpy(param.remote_addr.ip, recv_addr, sizeof(param.remote_addr.ip));
    strlcpy(param.local_addr.port, recv_port, sizeof(param.local_addr.port));

    dp_ctx = mcm_create_connection(&param);
    if (dp_ctx == NULL) {
        printf("Fail to connect to MCM data plane\n");
        exit(-1);
    }
    signal(SIGINT, intHandler);

    uint32_t frame_count = 0;
    uint32_t frm_size = width * height * 3 / 2; //TODO:assume it's NV12
    const uint32_t fps_interval = 30;
    double fps = 0.0;
    void *ptr = NULL;
    int timeout = -1;
    bool first_frame = true;
    float latency = 0;
    struct timespec ts_recv = {}, ts_send = {};
    struct timespec ts_begin = {}, ts_end = {};
    FILE* dump_fp = NULL;

    if (strlen(file_name) > 0) {
        dump_fp = fopen(file_name, "wb");
    }

    while (keepRunning) {
        /* receive frame */
        if (first_frame) {
            /* infinity for the 1st frame. */
            timeout = -1;
        } else {
            /* 1 second */
            timeout = 1000;
        }

        buf = mcm_dequeue_buffer(dp_ctx, timeout, &err);
        if (buf == NULL) {
            if (err == 0) {
                printf("Read buffer timeout\n");
            } else {
                printf("Failed to read buffer\n");
            }
            break;
        }
        // printf("INFO: buf->metadata.seq_num   = %d\n", buf->metadata.seq_num);
        // printf("INFO: buf->metadata.timestamp = %d\n", buf->metadata.timestamp);
        // printf("INFO: buf->len                = %ld\n", buf->len);

        clock_gettime(CLOCK_REALTIME, &ts_recv);
        if (first_frame) {
            ts_begin = ts_recv;
            first_frame = false;
        }

        if (strncmp(payload_type, "rtsp", sizeof(payload_type)) != 0) {
            if (dump_fp) {
                fwrite(buf->data, frm_size, 1, dump_fp);
            } else {
                // Following code are mainly for test purpose, it requires the sender side to
                // pre-set the first several bytes
                ptr = buf->data;
                if (*(uint32_t *)ptr != frame_count) {
                    printf("Wrong data content: expected %d, got %u\n", frame_count, *(uint32_t*)ptr);
                    /* catch up the sender frame count */
                    frame_count = *(uint32_t*)ptr;
                }
                ptr += sizeof(frame_count);
                ts_send = *(struct timespec *)ptr;

                if (frame_count % fps_interval == 0) {
                    /* calculate FPS */
                    clock_gettime(CLOCK_REALTIME, &ts_end);

                    fps = 1e9 * (ts_end.tv_sec - ts_begin.tv_sec);
                    fps += (ts_end.tv_nsec - ts_begin.tv_nsec);
                    fps /= 1e9;
                    fps = (double)fps_interval / fps;

                    clock_gettime(CLOCK_REALTIME, &ts_begin);
                }

                latency = 1000.0 * (ts_recv.tv_sec - ts_send.tv_sec);
                latency += (ts_recv.tv_nsec - ts_send.tv_nsec) / 1000000.0;

                printf("RX frames: [%u], latency: %0.1f ms, FPS: %0.3f\n", frame_count, latency, fps);
            }
        } else { //TODO: rtsp receiver side test code
            if (dump_fp) {
                fwrite(buf->data, buf->len, 1, dump_fp);
            }
            printf("RX package number:%d   seq_num: %d, timestamp: %u, RX H264 NALU: %ld\n", frame_count, buf->metadata.seq_num, buf->metadata.timestamp, buf->len);
        }

        frame_count++;

        /* enqueue buffer */
        if (mcm_enqueue_buffer(dp_ctx, buf) != 0) {
            break;
        }
    }

    /* Clean up */
    if (dump_fp) {
        fclose(dump_fp);
    }
    printf("Destroy MCM connection\n");
    mcm_destroy_connection(dp_ctx);

    return 0;
}
