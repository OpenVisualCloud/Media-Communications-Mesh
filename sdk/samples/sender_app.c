/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
#include "mcm_dp.h"

#define DEFAULT_SEND_IP "127.0.0.1"
#define DEFAULT_SEND_PORT "9001"
#define DEFAULT_TOTAL_NUM 300
#define DEFAULT_FRAME_WIDTH 1920
#define DEFAULT_FRAME_HEIGHT 1080
#define DEFAULT_FPS 30.0
#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"
#define DEFAULT_MEMIF_IS_MASTER 1
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

    fprintf(fp, "usage: %s [OPTION]\n", basename);
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
    fprintf(fp, "-s, --ip=ip_address\t\t"
                "Send data to IP address (default: %s)\n",
        DEFAULT_SEND_IP);
    fprintf(fp, "-p, --port=port_number\t\t"
                "Send data to Port (default: %s)\n",
        DEFAULT_SEND_PORT);
    fprintf(fp, "-o, --protocol=protocol_type\t"
                "Set protocol type (default: %s)\n",
        DEFAULT_PROTOCOL);
    fprintf(fp, "-n, --number=frame_number\t"
                "Total frame number to send (default: %d)\n",
        DEFAULT_TOTAL_NUM);
    fprintf(fp, "-i, --file=input_file\t\t"
                "Input file name (optional)\n");
    fprintf(fp, "-s, --socketpath=socket_path\t"
                "Set memif socket path (default: %s)\n",
        DEFAULT_MEMIF_SOCKET_PATH);
    fprintf(fp, "-m, --master=is_master\t\t"
                "Set memif conn is master (default: %d)\n",
        DEFAULT_MEMIF_IS_MASTER);
    fprintf(fp, "-i, --interfaceid=interface_id\t"
                "Set memif conn interface id (default: %d)\n",
        DEFAULT_MEMIF_INTERNFACE_ID);
    fprintf(fp, "\n");
}

int read_test_data(FILE* fp, mcm_buffer* buf, uint32_t width, uint32_t height, video_pixel_format pix_fmt)
{
    int ret = 0;
    static int frm_idx = 0;
    int frame_size = width * height * 3 / 2; //TODO: assume NV12

    assert(fp != NULL && buf != NULL);
    assert(buf->len >= frame_size);

    if (fread(buf->data, frame_size, 1, fp) < 1) {
        perror("Error when read frame file");
        ret = -1;
    }

    buf->metadata.seq_num = buf->metadata.timestamp = frm_idx++;
    buf->len = frame_size;

    return ret;
}

int gen_test_data(mcm_buffer* buf, uint32_t frame_count)
{
    /* operate on the buffer */
    void* ptr = buf->data;

    /* frame counter */
    *(uint32_t*)ptr = frame_count;
    ptr += sizeof(frame_count);

    /* timestamp */
    clock_gettime(CLOCK_REALTIME, (struct timespec*)ptr);

    return 0;
}

int main(int argc, char** argv)
{
    char send_addr[46] = DEFAULT_SEND_IP;
    char send_port[6] = DEFAULT_SEND_PORT;
    char input_file[128] = {0};
    char payload_type[32] = "";
    char protocol_type[32] = "";
    char socket_path[108] = DEFAULT_MEMIF_SOCKET_PATH;
    uint8_t is_master = DEFAULT_MEMIF_IS_MASTER;
    uint32_t interface_id = DEFAULT_MEMIF_INTERNFACE_ID;

    /* video resolution */
    uint32_t width = DEFAULT_FRAME_WIDTH;
    uint32_t height = DEFAULT_FRAME_HEIGHT;
    double vid_fps = DEFAULT_FPS;
    video_pixel_format pix_fmt = PIX_FMT_NV12; //PIX_FMT_YUV444M;

    mcm_conn_context* dp_ctx = NULL;
    mcm_conn_param param = { 0 };
    mcm_buffer* buf = NULL;
    uint32_t total_num = 300;

    int help_flag = 0;
    int opt;
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 'H' },
        { "width", required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "fps", required_argument, NULL, 'f' },
        { "ip", required_argument, NULL, 's' },
        { "port", required_argument, NULL, 'p' },
        { "protocol", required_argument, NULL, 'o' },
        { "number", required_argument, NULL, 'n' },
        { "file", required_argument, NULL, 'i' },
        { "type", required_argument, NULL, 't' },
        { "socketpath", required_argument, NULL, 'k' },
        { "master", required_argument, NULL, 'm' },
        { "interfaceid", required_argument, NULL, 'd' },
        { 0 }
    };

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "Hw:h:f:s:p:o:n:i:t:k:m:d:", longopts, 0);
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
        case 's':
            strlcpy(send_addr, optarg, sizeof(send_addr));
            break;
        case 'p':
            strlcpy(send_port, optarg, sizeof(send_port));
            break;
        case 'o':
            strlcpy(protocol_type, optarg, sizeof(protocol_type));
            break;
        case 'n':
            total_num = atoi(optarg);
            break;
        case 'i':
            strlcpy(input_file, optarg, sizeof(input_file));
            break;
        case 't':
            strlcpy(payload_type, optarg, sizeof(payload_type));
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

    /* is sender */
    param.type = is_tx;
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
    case PAYLOAD_TYPE_ST20_VIDEO:
    case PAYLOAD_TYPE_ST22_VIDEO:
    default:
        /* video format */
        param.payload_args.video_args.width   = param.width = width;
        param.payload_args.video_args.height  = param.height = height;
        param.payload_args.video_args.fps     = param.fps = vid_fps;
        param.payload_args.video_args.pix_fmt = param.pix_fmt = pix_fmt;
        break;
    }

    strlcpy(param.remote_addr.ip, send_addr, sizeof(param.remote_addr.ip));
    strlcpy(param.remote_addr.port, send_port, sizeof(param.remote_addr.port));

    dp_ctx = mcm_create_connection(&param);
    if (dp_ctx == NULL) {
        printf("Fail to connect to MCM data plane\n");
        exit(-1);
    }
    signal(SIGINT, intHandler);

    FILE* input_fp = NULL;
    uint32_t frame_count = 0;
    const uint32_t fps_interval = 30;
    double fps = 0.0;
    struct timespec ts_begin = {}, ts_end = {};
    // struct timespec ts_frame_begin = {}, ts_frame_end = {};

    if (strlen(input_file) > 0) {
        struct stat statbuf = { 0 };
        if (stat(input_file, &statbuf) == -1) {
            perror(NULL);
            exit(-1);
        }

        input_fp = fopen(input_file, "rb");
        if (input_fp == NULL) {
            printf("Fail to open input file: %s\n", input_file);
            exit(-1);
        }
    }

    // const useconds_t pacing = 1000000.0 / vid_fps;
    while (keepRunning) {
        /* Timestamp for frame start. */

        buf = mcm_dequeue_buffer(dp_ctx, -1, NULL);
        if (buf == NULL) {
            break;
        }
        // printf("INFO: buf->metadata.seq_num = %d\n", buf->metadata.seq_num);
        // printf("INFO: buf->metadata.timestamp = %d\n", buf->metadata.timestamp);
        // printf("INFO: buf->len = %ld\n", buf->len);

        if (input_fp == NULL) {
            gen_test_data(buf, frame_count);
        } else {
            if (read_test_data(input_fp, buf, width, height, pix_fmt) < 0) {
                break;
            }
        }

        if (mcm_enqueue_buffer(dp_ctx, buf) != 0) {
            break;
        }

        if (frame_count % fps_interval == 0) {
            /* calculate FPS */
            clock_gettime(CLOCK_REALTIME, &ts_end);

            fps = 1e9 * (ts_end.tv_sec - ts_begin.tv_sec);
            fps += (ts_end.tv_nsec - ts_begin.tv_nsec);
            fps /= 1e9;
            fps = (double)fps_interval / fps;

            clock_gettime(CLOCK_REALTIME, &ts_begin);
        }

        printf("TX frames: [%d], FPS: %0.2f\n", frame_count, fps);

        frame_count++;

        if (frame_count >= total_num) {
            break;
        }

        /* Timestamp for frame end. */
        // clock_gettime(CLOCK_REALTIME, &ts_frame_end);

        /* sleep for 1/fps */
        // useconds_t spend = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) + (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec)/1000;
        // printf("pacing: %d\n", pacing);
        // printf("spend: %d\n", spend);
        // if (pacing > spend) {
        //     // printf("sleep: %d\n", pacing - spend);
        //     usleep(pacing - spend);
        // }
        // usleep(pacing);
    }

    /* Clean up */
    mcm_destroy_connection(dp_ctx);

    if (input_fp != NULL) {
        fclose(input_fp);
    }

    return 0;
}
