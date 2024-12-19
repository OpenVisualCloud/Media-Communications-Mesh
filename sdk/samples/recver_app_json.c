/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsd/string.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mesh_dp.h"

#define RECV_LOCAL_FILE "recv.yuv"
#define RECV_JSON_FILE "recv.json"

static volatile bool keepRunning = true;

void intHandler(int dummy) { keepRunning = 0; }

size_t get_file_size(FILE *f) {
    if (f == NULL) {
        return 0;
    }
    long prev = ftell(f);
    if (prev < 0) {
        return 0;
    }
    if (fseek(f, (long)0, (long)SEEK_END) != 0) {
        (void)fseek(f, prev, (long)SEEK_SET);
        return 0;
    } else {
        long size = ftell(f);
        (void)fseek(f, prev, (long)SEEK_SET);
        if (size < 0) {
            return 0;
        }
        return (size_t)size;
    }
}

/* print a description of all supported options */
void usage(FILE *fp, const char *path) {
    /* take only the last portion of the path */
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(fp, "Usage: %s [OPTION]\n", basename);
    fprintf(fp, "-H, --help\t\t\t"
                "Print this help and exit\n");
    fprintf(fp,
            "-j, --json=file_name\t"
            "JSON file with receiver configuration(example: %s)\n",
            RECV_JSON_FILE);
    fprintf(fp,
            "-o, --outputfile=file_name\t"
            "Save stream to local file (example: %s)\n",
            RECV_LOCAL_FILE);
    fprintf(fp, "\n");
}

int main(int argc, char **argv) {
    char output_filename[128] = "";
    FILE *outputfile = NULL;

    char json_filename[128] = "";
    FILE *jsonfile = NULL;
    char *json_config = NULL;

    MeshClient *client = NULL;
    MeshConnection *conn = NULL;
    MeshBuffer *buf = NULL;
    int err;

    int help_flag = 0;
    int opt;
    struct option longopts[] = {{"help", no_argument, &help_flag, 'H'},
                                {"outputfile", required_argument, NULL, 'o'},
                                {"json", required_argument, NULL, 'j'},
                                {0}};

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "H:o:j:", longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'H':
            help_flag = 1;
            break;
        case 'o':
            strlcpy(output_filename, optarg, sizeof(output_filename));
            break;
        case 'j':
            strlcpy(json_filename, optarg, sizeof(json_filename));
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

    jsonfile = fopen(json_filename, "rb");
    if (!jsonfile) {
        fprintf(stderr, "Invalid json file \n");
        err = -1;
        goto fail;
    }

    const size_t json_filesize = get_file_size(jsonfile);
    if (json_filesize == 0) {
        fprintf(stderr, "Json file empty \n");
        err = -1;
        goto fail;
    }

    json_config = calloc(1, json_filesize + 1);
    if (!json_config) {
        fprintf(stderr, "Failed to allocate memory for json config \n");
        err = -1;
        goto fail;
    }

    if (fread(json_config, 1, json_filesize, jsonfile) != json_filesize) {
        fprintf(stderr, "Failed to read json file \n");
        err = -1;
        goto fail;
    }

    if (strlen(output_filename) > 0) {
        outputfile = fopen(output_filename, "wb");
        if (!outputfile) {
            fprintf(stderr, "Cannot create output file \n");
            err = -1;
            goto fail;
        }
    }

    err = mesh_create_client_json(&client, json_config);
    if (err) {
        printf("Failed to create a mesh client: %s (%d)\n", mesh_err2str(err), err);
        goto fail;
    }

    err = mesh_create_rx_connection(client, &conn, json_config);
    if (err) {
        printf("Failed to create a mesh connection: %s (%d)\n", mesh_err2str(err), err);
        goto fail;
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

        printf("INFO: buf->len = %ld frame size = %u\n", buf->payload_len, frm_size);

        clock_gettime(CLOCK_REALTIME, &ts_recv);
        if (first_frame) {
            ts_begin = ts_recv;
            first_frame = false;
        }

        if (outputfile) {
            fwrite(buf->payload_ptr, buf->payload_len, 1, outputfile);
        } else {
            // Following code are mainly for test purpose, it requires the sender side to
            // pre-set the first several bytes
            ptr = buf->payload_ptr;
            if (*(uint32_t *)ptr != frame_count) {
                printf("Wrong data content: expected %u, got %u\n", frame_count, *(uint32_t *)ptr);
                /* catch up the sender frame count */
                frame_count = *(uint32_t *)ptr;
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
        printf("Throughput: %.2lf MB/s, %.2lf Gb/s \n", throughput_MB, throughput_MB * 8 / 1000);

        frame_count++;

        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        printf("\n");
    }

fail:
    if (conn) {
        mesh_delete_connection(&conn);
    }
    if (client) {
        mesh_delete_client(&client);
    }
    if (json_config) {
        free(json_config);
    }
    if (jsonfile) {
        fclose(jsonfile);
    }
    if (outputfile) {
        fclose(outputfile);
    }
    return err;
}
