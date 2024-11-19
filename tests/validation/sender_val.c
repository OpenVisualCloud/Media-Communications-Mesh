/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

static volatile bool keepRunning = true;
bool loop = DEFAULT_INFINITE_LOOP;

void intHandler(int dummy)
{
    keepRunning = 0;
}

int read_test_data(FILE* fp, MeshBuffer* buf, uint32_t frame_size)
{
    int ret = 0;

    assert(fp != NULL && buf != NULL);
    assert(buf->data_len >= frame_size);

    if (fread(buf->data, frame_size, 1, fp) < 1)
        ret = -1;
    return ret;
}

int gen_test_data(MeshBuffer *buf, uint32_t frame_count)
{
    /* operate on the buffer */
    void* ptr = buf->data;

    /* frame counter */
    *(uint32_t*)ptr = frame_count;
    ptr += sizeof(frame_count);

    /* timestamp */
    clock_gettime( CLOCK_REALTIME , (struct timespec*)ptr);

    return 0;
}

int main(int argc, char** argv)
{
    char recv_addr[MESH_IP_ADDRESS_SIZE] = DEFAULT_RECV_IP;
    char recv_port[5] = DEFAULT_RECV_PORT;
    char send_addr[MESH_IP_ADDRESS_SIZE] = DEFAULT_SEND_IP;
    char send_port[5] = DEFAULT_SEND_PORT;
    char protocol_type[32] = DEFAULT_PROTOCOL;
    char payload_type[32] = DEFAULT_PAYLOAD_TYPE;

    static char file_name[128] = "";

    char pix_fmt_string[32] = DEFAULT_VIDEO_FMT;
    char socket_path[108] = DEFAULT_MEMIF_SOCKET_PATH;
    uint32_t interface_id = DEFAULT_MEMIF_INTERFACE_ID;

    /* video resolution */
    uint32_t width = DEFAULT_FRAME_WIDTH;
    uint32_t height = DEFAULT_FRAME_HEIGHT;
    double vid_fps = DEFAULT_FPS;
    uint32_t total_num = 300;

    MeshClient *client;
    MeshConnection *conn;
    MeshBuffer *buf;
    int err;

    int help_flag = 0;
    int opt;
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 'H' },

        { "file_name", required_argument, NULL, 'b' },
        { "width", required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "fps", required_argument, NULL, 'f' },
        { "pix_fmt", required_argument, NULL, 'x' },
        
        { "recv_ip", required_argument, NULL, 'r' },
        { "recv_port", required_argument, NULL, 'i' },
        { "send_ip", required_argument, NULL, 's' },
        { "send_port", required_argument, NULL, 'p' },
        
        { "protocol_type", required_argument, NULL, 'o' },
        { "payload_type", required_argument, NULL, 't' },
        { "socketpath", required_argument, NULL, 'k' },
        { "interfaceid", required_argument, NULL, 'd' },

        { "loop", required_argument, NULL, 'l' },
        { 0 }
    };

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "Hb:w:h:f:x:r:i:s:p:o:t:k:d:l:", longopts, 0);
        if (opt == -1)
            break;

        switch (opt) {
        case 'H':
            usage(stdout, argv[0], 1);
            return 0;
        case 'b':
            strlcpy(file_name, optarg, sizeof(file_name));
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
        case 'x':
            strlcpy(pix_fmt_string, optarg, sizeof(pix_fmt_string));
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
        case 'k':
            strlcpy(socket_path, optarg, sizeof(socket_path));
            break;
        case 'd':
            interface_id = atoi(optarg);
            break;
        case 'l':
            loop = (atoi(optarg)>0);
            break;
        default:
            break;
        }
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

        set_video_pix_fmt(&cfg.pixel_format, pix_fmt_string);

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

    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    if (err) {
        printf("Failed to establish connection: %s (%d)\n",
               mesh_err2str(err), err);
        goto error_delete_conn;
    }

    signal(SIGINT, intHandler);

    uint32_t frame_count = 0;
    uint32_t frame_size = conn->buf_size;

    FILE* input_fp = NULL;

    const uint32_t stat_interval = 10;
    double fps = 0.0;
    double throughput_MB = 0;
    double stat_period_s = 0;

    struct timespec ts_begin = {}, ts_end = {};
    struct timespec ts_frame_begin = {}, ts_frame_end = {};

    if (strlen(file_name) > 0) {
        struct stat statbuf = { 0 };
        if (stat(file_name, &statbuf) == -1) {
            perror(NULL);
            goto error_delete_conn;
        }

        input_fp = fopen(file_name, "rb");
        if (input_fp == NULL) {
            printf("Failed to open input file: %s\n", file_name);
            goto error_delete_conn;
        }
    }

    const __useconds_t pacing = 1000000.0 / vid_fps;
    while (keepRunning) {
        /* Timestamp for frame start. */
        clock_gettime(CLOCK_REALTIME, &ts_frame_begin);

        err = mesh_get_buffer(conn, &buf);
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        printf("INFO: frame_size = %u\n", frame_size);

        if (input_fp == NULL) {
            gen_test_data(buf, frame_count);
        } else {
            if (read_test_data(input_fp, buf, frame_size) < 0) {
                if (input_fp != NULL) {
                    fclose(input_fp);
                    input_fp = NULL;
                }
                if (loop) {
                    input_fp = fopen(file_name, "rb");
                    if (input_fp == NULL) {
                        printf("Failed to open input file for infinite loop: %s\n",
                               file_name);
                        break;
                    }
                    if (read_test_data(input_fp, buf, frame_size) < 0)
                        break;
                } else {
                    break;
                }
            }
        }

        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        if (frame_count % stat_interval == 0) {
            /* calculate FPS */
            clock_gettime(CLOCK_REALTIME, &ts_end);

            stat_period_s = (ts_end.tv_sec - ts_begin.tv_sec);
            stat_period_s += (ts_end.tv_nsec - ts_begin.tv_nsec) / 1e9;
            fps = stat_interval / stat_period_s;
            throughput_MB = fps * frame_size / 1000000;

            clock_gettime(CLOCK_REALTIME, &ts_begin);
        }

        printf("TX frames: [%d], FPS: %0.2f [%0.2f]\n", frame_count, fps, vid_fps);
        printf("Throughput: %.2lf MB/s, %.2lf Gb/s \n", throughput_MB,  throughput_MB * 8 / 1000);

        frame_count++;

        if (frame_count >= total_num)
            break;

        /* Timestamp for frame end. */
        clock_gettime(CLOCK_REALTIME, &ts_frame_end);

        /* sleep for 1/fps */
        __useconds_t spend = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) + (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec)/1000;
        printf("pacing: %d\n", pacing);
        printf("spend: %d\n", spend);

        printf("\n");
    }

    sleep(2);

    /* Clean up */
    err = mesh_delete_connection(&conn);
    if (err)
        printf("Failed to delete connection: %s (%d)\n", mesh_err2str(err), err);

    err = mesh_delete_client(&client);
    if (err)
        printf("Failed to delete mesh client: %s (%d)\n", mesh_err2str(err), err);

    if (input_fp != NULL)
        fclose(input_fp);

    return 0;

error_delete_conn:
    mesh_delete_connection(&conn);

error_delete_client:
    mesh_delete_client(&client);
    exit(-1);
}
