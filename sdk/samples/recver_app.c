/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
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
#include "mesh_dp.h"

#define DEFAULT_RECV_IP "127.0.0.1"
#define DEFAULT_RECV_PORT "9001"
#define DEFAULT_SEND_IP "127.0.0.1"
#define DEFAULT_SEND_PORT "9001"
#define DEFAULT_FRAME_WIDTH 1920
#define DEFAULT_FRAME_HEIGHT 1080
#define DEFAULT_FPS 30.0
#define DEFAULT_PAYLOAD_TYPE "st20"
#define DEFAULT_LOCAL_FILE "data-sdk.264"
#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"
#define DEFAULT_MEMIF_INTERFACE_ID 0
#define DEFAULT_PROTOCOL "auto"
#define DEFAULT_VIDEO_FMT "yuv422p10le"

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
    fprintf(fp, "-d, --interfaceid=interface_id\t"
                "Set memif conn interface id (default: %d)\n",
        DEFAULT_MEMIF_INTERFACE_ID);
    fprintf(fp, "-x, --pix_fmt=mcm_pix_fmt\t"
                "Set pix_fmt conn color format (default: %s)\n",
        DEFAULT_VIDEO_FMT);
    fprintf(fp, "\n");
}

int main(int argc, char** argv)
{
    char recv_addr[46] = DEFAULT_RECV_IP;
    char recv_port[6] = DEFAULT_RECV_PORT;
    char send_addr[46] = DEFAULT_SEND_IP;
    char send_port[6] = DEFAULT_SEND_PORT;

    char protocol_type[32] = DEFAULT_PROTOCOL;
    char payload_type[32] = DEFAULT_PAYLOAD_TYPE;
    char file_name[128] = "";
    char pix_fmt_string[32] = DEFAULT_VIDEO_FMT;
    char socket_path[108] = DEFAULT_MEMIF_SOCKET_PATH;
    uint32_t interface_id = DEFAULT_MEMIF_INTERFACE_ID;

    /* video resolution */
    uint32_t width = DEFAULT_FRAME_WIDTH;
    uint32_t height = DEFAULT_FRAME_HEIGHT;
    double vid_fps = DEFAULT_FPS;

    MeshClient *client;
    MeshConnection *conn;
    MeshBuffer *buf;
    int err;

    int help_flag = 0;
    int opt;
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 'H' },
        { "width", required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "fps", required_argument, NULL, 'f' },
        { "rcv_ip", required_argument, NULL, 'r' },
        { "rcv_port", required_argument, NULL, 'i' },
        { "send_ip", required_argument, NULL, 's' },
        { "send_port", required_argument, NULL, 'p' },
        { "protocol", required_argument, NULL, 'o' },
        { "type", required_argument, NULL, 't' },
        { "socketpath", required_argument, NULL, 'k' },
        { "interfaceid", required_argument, NULL, 'd' },
        { "dumpfile", required_argument, NULL, 'b' },
        { "pix_fmt", required_argument, NULL, 'x' },
        { 0 }
    };

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "Hw:h:f:r:i:s:p:o:t:s:k:m:d:x:b:", longopts, 0);
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
        case 'i':
            strlcpy(recv_port, optarg, sizeof(recv_port));
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
        case 't':
            strlcpy(payload_type, optarg, sizeof(payload_type));
            break;
        case 'b':
            strlcpy(file_name, optarg, sizeof(file_name));
            break;
        case 'k':
            strlcpy(socket_path, optarg, sizeof(socket_path));
            break;
        case 'd':
            interface_id = atoi(optarg);
            break;
        case 'x':
            strlcpy(pix_fmt_string, optarg, sizeof(pix_fmt_string));
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

    err = mesh_create_client(&client, NULL);
    if (err) {
        printf("Failed to create a mesh client: %s (%d)\n",
               mesh_err2str(err), err);
        exit(-1);
    }

    err = mesh_create_connection(client, &conn);
    if (err) {
        printf("Failed to create a mesh connection: %s (%d)\n",
               mesh_err2str(err), err);
        goto error_delete_client;
    }

    /* protocol type */
    if (!strcmp(protocol_type, "memif")) {
        MeshConfig_Memif cfg;

        strlcpy(cfg.socket_path, socket_path, sizeof(cfg.socket_path));
        cfg.interface_id = interface_id;

        err = mesh_apply_connection_config_memif(conn, &cfg);
        if (err) {
            printf("Failed to apply memif configuration: %s (%d)\n",
               mesh_err2str(err), err);
            goto error_delete_conn;
        }
    } else {
        if (!strcmp(payload_type, "rdma")) {
            MeshConfig_RDMA cfg;

            strlcpy(cfg.remote_ip_addr, send_addr, sizeof(cfg.remote_ip_addr));
            cfg.remote_port = atoi(send_port);
            strlcpy(cfg.local_ip_addr, recv_addr, sizeof(cfg.local_ip_addr));
            cfg.local_port = atoi(recv_port);

            err = mesh_apply_connection_config_rdma(conn, &cfg);
            if (err) {
                printf("Failed to apply RDMA configuration: %s (%d)\n",
                    mesh_err2str(err), err);
                goto error_delete_conn;
            }
        } else {
            MeshConfig_ST2110 cfg;

            strlcpy(cfg.remote_ip_addr, send_addr, sizeof(cfg.remote_ip_addr));
            cfg.remote_port = atoi(send_port);
            strlcpy(cfg.local_ip_addr, recv_addr, sizeof(cfg.local_ip_addr));
            cfg.local_port = atoi(recv_port);

            /* transport type */
            if (!strcmp(payload_type, "st20")) {
                cfg.transport = MESH_CONN_TRANSPORT_ST2110_20;
            } else if (!strcmp(payload_type, "st22")) {
                cfg.transport = MESH_CONN_TRANSPORT_ST2110_22;
            } else if (!strcmp(payload_type, "st30")) {
                cfg.transport = MESH_CONN_TRANSPORT_ST2110_30;
            } else {
                printf("Unknown SMPTE ST2110 transport type: %s\n", payload_type);
                goto error_delete_conn;
            }

            err = mesh_apply_connection_config_st2110(conn, &cfg);
            if (err) {
                printf("Failed to apply SMPTE ST2110 configuration: %s (%d)\n",
                    mesh_err2str(err), err);
                goto error_delete_conn;
            }
        }
    }

    /* payload type */
    if (!strcmp(payload_type, "st20") || !strcmp(payload_type, "st22") ||
        !strcmp(payload_type, "rdma")) {
        /* video */
        MeshConfig_Video cfg;

        if (!strncmp(pix_fmt_string, "yuv422p10le", sizeof(pix_fmt_string)))
            cfg.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE;
        else if (!strncmp(pix_fmt_string, "v210", sizeof(pix_fmt_string)))
            cfg.pixel_format = MESH_VIDEO_PIXEL_FORMAT_V210;
        else if (!strncmp(pix_fmt_string, "yuv422p10rfc4175", sizeof(pix_fmt_string)))
            cfg.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10;
        else {
            printf("Unknown video pixel format: %s\n", pix_fmt_string);
            goto error_delete_conn;
        }

        cfg.width = width;
        cfg.height = height;
        cfg.fps = vid_fps;

        err = mesh_apply_connection_config_video(conn, &cfg);
        if (err) {
            printf("Failed to apply video configuration: %s (%d)\n",
                   mesh_err2str(err), err);
            goto error_delete_conn;
        }
    } else if (!strcmp(payload_type, "st30")) {
        /* audio */
        MeshConfig_Audio cfg;

        cfg.channels = 2;
        cfg.format = MESH_AUDIO_FORMAT_PCM_S16BE;
        cfg.sample_rate = MESH_AUDIO_SAMPLE_RATE_48000;
        cfg.packet_time = MESH_AUDIO_PACKET_TIME_1MS;

        err = mesh_apply_connection_config_audio(conn, &cfg);
        if (err) {
            printf("Failed to apply audio configuration: %s (%d)\n",
                   mesh_err2str(err), err);
            goto error_delete_conn;
        }
    } else {
        printf("Unknown payload type: %s\n", payload_type);
        goto error_delete_conn;
    }

    err = mesh_establish_connection(conn, MESH_CONN_KIND_RECEIVER);
    if (err) {
        printf("Failed to establish connection: %s (%d)\n",
               mesh_err2str(err), err);
        goto error_delete_conn;
    }

    signal(SIGINT, intHandler);

    uint32_t frame_count = 0;
    uint32_t frm_size = conn->buf_size;

    const uint32_t stat_interval = 10;
    double fps = 0.0;
    double throughput_MB = 0;
    double stat_period_s = 0;
    void *ptr = NULL;
    int timeout = MESH_TIMEOUT_INFINITE;
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
            /* infinite for the 1st frame. */
            timeout = MESH_TIMEOUT_INFINITE;
        } else {
            /* 1 second */
            timeout = 1000;
        }

        err = mesh_get_buffer_timeout(conn, &buf, timeout);
        if (err == -MESH_ERR_CONN_CLOSED) {
            printf("Connection closed\n");
            break;
        }
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        printf("INFO: buf->len = %ld frame size = %u\n", buf->data_len, frm_size);

        clock_gettime(CLOCK_REALTIME, &ts_recv);
        if (first_frame) {
            ts_begin = ts_recv;
            first_frame = false;
        }

        if (dump_fp) {
            fwrite(buf->data, buf->data_len, 1, dump_fp);
        } else {
            // Following code are mainly for test purpose, it requires the sender side to
            // pre-set the first several bytes
            ptr = buf->data;
            if (*(uint32_t *)ptr != frame_count) {
                printf("Wrong data content: expected %u, got %u\n", frame_count, *(uint32_t*)ptr);
                /* catch up the sender frame count */
                frame_count = *(uint32_t*)ptr;
            }
            ptr += sizeof(frame_count);
            ts_send = *(struct timespec *)ptr;

            latency = 1000.0 * (ts_recv.tv_sec - ts_send.tv_sec);
            latency += (ts_recv.tv_nsec - ts_send.tv_nsec) / 1000000.0;
        }

        if (frame_count % stat_interval == 0) {
            /* calculate FPS */
            clock_gettime(CLOCK_REALTIME, &ts_end);

            stat_period_s = (ts_end.tv_sec - ts_begin.tv_sec);
            stat_period_s += (ts_end.tv_nsec - ts_begin.tv_nsec) / 1e9;
            fps = stat_interval / stat_period_s;
            throughput_MB = fps * frm_size / 1000000;

            clock_gettime(CLOCK_REALTIME, &ts_begin);
        }
        printf("RX frames: [%u], latency: %0.1f ms, FPS: %0.3f\n", frame_count, latency, fps);
        printf("Throughput: %.2lf MB/s, %.2lf Gb/s \n", throughput_MB,  throughput_MB * 8 / 1000);

        frame_count++;

        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        printf("\n");
    }

    /* Clean up */
    if (dump_fp) {
        fclose(dump_fp);
    }

    err = mesh_delete_connection(&conn);
    if (err)
        printf("Failed to delete connection: %s (%d)\n", mesh_err2str(err), err);

    err = mesh_delete_client(&client);
    if (err)
        printf("Failed to delete mesh client: %s (%d)\n", mesh_err2str(err), err);

    return 0;

error_delete_conn:
    mesh_delete_connection(&conn);

error_delete_client:
    mesh_delete_client(&client);
    exit(-1);
}
