/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define _GNU_SOURCE
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
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#include "pingpong_common.h"

#define DEFAULT_RECV_PORT "10001"
#define DEFAULT_SEND_PORT "9001"

static volatile bool keepRunning = true;

static MeshClient *client;
atomic_int counter = 0;


Config config = {
    .recv_addr = DEFAULT_RECV_IP,
    .recv_port = DEFAULT_RECV_PORT,
    .send_addr = DEFAULT_SEND_IP,
    .send_port = DEFAULT_SEND_PORT,
    
    .payload_type = "",
    .protocol_type = "",
    .pix_fmt_string = DEFAULT_VIDEO_FMT,
    .socket_path = DEFAULT_MEMIF_SOCKET_PATH,
    .interface_id = DEFAULT_MEMIF_INTERFACE_ID,
    .loop = DEFAULT_INFINITE_LOOP,

    .width = DEFAULT_FRAME_WIDTH,
    .height = DEFAULT_FRAME_HEIGHT,
    .vid_fps = DEFAULT_FPS,
    .frame_size = 0,
    .total_num = 300
};

void intHandler(int dummy) { keepRunning = 0; }

/* print a description of all supported options */
void usage(FILE *fp, const char *path)
{
    /* take only the last portion of the path */
    const char *basename = strrchr(path, '/');
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
    fprintf(fp, "-s, --socketpath=socket_path\t"
                "Set memif socket path (default: %s)\n",
        DEFAULT_MEMIF_SOCKET_PATH);
    fprintf(fp, "-d, --interfaceid=interface_id\t"
                "Set memif conn interface id (default: %d)\n",
        DEFAULT_MEMIF_INTERFACE_ID);
    fprintf(fp, "-l, --loop=is_loop\t"
                "Set infinite loop sending (default: %d)\n",
        DEFAULT_INFINITE_LOOP);
    fprintf(fp, "\n");
}

void * pong_thread(void *arg)
{
    int thread_id = *(int *)arg;
    cpu_set_t cpuset;
    int err;
    int timeout_ms = -1;

    MeshConnection *s_conn;
    MeshConnection *r_conn;
    MeshBuffer *s_buf;
    MeshBuffer *r_buf;

    err = mesh_create_connection(client, &r_conn);
    if (err) {
        printf("Failed to create a recv mesh connection: %s (%d)\n", mesh_err2str(err), err);
        return NULL;
    }

    err = init_conn(r_conn, &config, MESH_CONN_KIND_RECEIVER, thread_id);
    if (err) {
        printf("ERROR: init_conn failed \n");
        mesh_delete_connection(&r_conn);
        return NULL;
    }
    usleep(100000);

    err = mesh_create_connection(client, &s_conn);
    if (err) {
        printf("Failed to create a send mesh connection: %s (%d)\n", mesh_err2str(err), err);
        return NULL;
    }

    err = init_conn(s_conn, &config, MESH_CONN_KIND_SENDER, thread_id);
    if (err) {
        printf("ERROR: init_conn failed \n");
        mesh_delete_connection(&s_conn);
        return NULL;
    }

    // Pin the thread to specific CPU core
    CPU_ZERO(&cpuset);
    CPU_SET((thread_id + config.threads_num * 2) % CPU_CORES, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np");
        return NULL;
    }

    while (1) {
        err = mesh_get_buffer_timeout(r_conn, &r_buf, timeout_ms);
        if (err == -MESH_ERR_CONN_CLOSED) {
            printf("Connection closed\n");
            break;
        }
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
        timeout_ms = 1000000.0 / config.vid_fps + 1000;

        err = mesh_get_buffer(s_conn, &s_buf);
        if (err) {
            printf("Failed to get send buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        memcpy(s_buf->data, r_buf->data, sizeof(struct timespec) + sizeof(int));

        err = mesh_put_buffer(&s_buf);
        if (err) {
            printf("Failed to put send buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        err = mesh_put_buffer(&r_buf);
        if (err) {
            printf("Failed to put recv buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
    }

    sleep(1);

    /* Clean up */
    err = mesh_delete_connection(&s_conn);
    if (err)
        printf("Failed to delete send connection: %s (%d)\n", mesh_err2str(err), err);
    err = mesh_delete_connection(&r_conn);
    if (err)
        printf("Failed to delete recv connection: %s (%d)\n", mesh_err2str(err), err);

    return NULL;
}

int main(int argc, char **argv)
{
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
        { "number", required_argument, NULL, 'n' },
        { "file", required_argument, NULL, 'b' },
        { "type", required_argument, NULL, 't' },
        { "socketpath", required_argument, NULL, 'k' },
        { "interfaceid", required_argument, NULL, 'd' },
        { "loop", required_argument, NULL, 'l' },
        { "pix_fmt", required_argument, NULL, 'x' },
        { "threads_num", required_argument, NULL, 'm' },
        { 0 }
    };

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "Hw:h:f:s:p:o:n:r:i:t:k:d:l:x:b:m:", longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'H':
            help_flag = 1;
            break;
        case 'w':
            config.width = atoi(optarg);
            break;
        case 'h':
            config.height = atoi(optarg);
            break;
        case 'f':
            config.vid_fps = atof(optarg);
            break;
        case 'r':
            strlcpy(config.recv_addr, optarg, sizeof(config.recv_addr));
            break;
        case 'i':
            strlcpy(config.recv_port, optarg, sizeof(config.recv_port));
            break;
        case 's':
            strlcpy(config.send_addr, optarg, sizeof(config.send_addr));
            break;
        case 'p':
            strlcpy(config.send_port, optarg, sizeof(config.send_port));
            break;
        case 'o':
            strlcpy(config.protocol_type, optarg, sizeof(config.protocol_type));
            break;
        case 'n':
            config.total_num = atoi(optarg);
            break;
        case 't':
            strlcpy(config.payload_type, optarg, sizeof(config.payload_type));
            break;
        case 'k':
            strlcpy(config.socket_path, optarg, sizeof(config.socket_path));
            break;
        case 'd':
            config.interface_id = atoi(optarg);
            break;
        case 'l':
            config.loop = (atoi(optarg) > 0);
            break;
        case 'x':
            strlcpy(config.pix_fmt_string, optarg, sizeof(config.pix_fmt_string));
            break;
        case 'm':
            config.threads_num = atoi(optarg);
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
    signal(SIGINT, intHandler);

    err = mesh_create_client(&client, NULL);
    if (err) {
        printf("Failed to create a mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(-1);
    }

    pthread_t *pong_threads = malloc(config.threads_num * sizeof(pthread_t *));
    int *threads_id = malloc(config.threads_num * sizeof(int *));

    for (int i = 0; i < config.threads_num; i++) {
        threads_id[i] = i;
        if (pthread_create(&pong_threads[i], NULL, pong_thread, &threads_id[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
        usleep(100000);
    }

    // Wait for all threads to finish
    for (int i = 0; i < config.threads_num; i++) {
        pthread_join(pong_threads[i], NULL);
    }

    free(pong_threads);
    free(threads_id);

    return 0;

    mesh_delete_client(&client);

    exit(-1);
}
