/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <bsd/string.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "mesh_dp.h"

#define SENDER_LOCAL_FILE "sender.yuv"
#define SENDER_JSON_FILE "sender.json"

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
            "JSON file with sender configuration(example: %s)\n",
            SENDER_JSON_FILE);
    fprintf(fp,
            "-i, --iputfile=file_name\t"
            "Input file to send (example: %s)\n",
            SENDER_LOCAL_FILE);
    fprintf(fp, "-n, --number=frame_number\t"
                "Number of fraems to be sent, (default: -1, infinite)\n");
}

int read_test_data(FILE *fp, MeshBuffer *buf, uint32_t frame_size) {
    int ret = 0;

    assert(fp != NULL && buf != NULL);
    assert(buf->data_len >= frame_size);

    if (fread(buf->data, frame_size, 1, fp) < 1) {
        ret = -1;
    }
    return ret;
}

int gen_test_data(MeshBuffer *buf, uint32_t frame_count) {
    /* operate on the buffer */
    void *ptr = buf->data;

    /* frame counter */
    *(uint32_t *)ptr = frame_count;
    ptr += sizeof(frame_count);

    /* timestamp */
    clock_gettime(CLOCK_REALTIME, (struct timespec *)ptr);

    return 0;
}

int main(int argc, char **argv) {
    int32_t frame_num = -1;

    char input_filename[128] = "";
    FILE *inputfile = NULL;

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
                                {"iputfile", optional_argument, NULL, 'i'},
                                {"json", required_argument, NULL, 'j'},
                                {"number", optional_argument, NULL, 'n'},
                                {0}};

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "H:i:j:n:", longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'H':
            help_flag = 1;
            break;
        case 'i':
            strlcpy(input_filename, optarg, sizeof(input_filename));
            break;
        case 'j':
            strlcpy(json_filename, optarg, sizeof(json_filename));
            break;
        case 'n':
            frame_num = atoi(optarg);
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

    if (strlen(input_filename) > 0) {
        inputfile = fopen(input_filename, "rb");
        if (!inputfile) {
            fprintf(stderr, "Cannot open input file \n");
            err = -1;
            goto fail;
        }
    } else {
        fprintf(stderr, "Warning: Input file not provided, generating data\n");
    }

    if (frame_num < 0) {
        frame_num = -1;
        fprintf(stderr, "Warning: Negative frame count provided, sending infinite\n");
    }

    err = mesh_create_client_json(&client, json_config);
    if (err) {
        printf("Failed to create a mesh client: %s (%d)\n", mesh_err2str(err), err);
        goto fail;
    }

    err = mesh_create_tx_connection(client, &conn, json_config);
    if (err) {
        printf("Failed to create a mesh connection: %s (%d)\n", mesh_err2str(err), err);
        goto fail;
    }

    uint32_t frame_size = conn->buf_size;

    signal(SIGINT, intHandler);

    uint32_t frames_processed = 0;
    const uint32_t stat_interval = 10;
    double fps = 0.0;
    double throughput_MB = 0;
    double stat_period_s = 0;
    struct timespec ts_begin = {}, ts_end = {};
    struct timespec ts_frame_begin = {}, ts_frame_end = {};

    while (keepRunning) {
        /* Timestamp for frame start. */
        clock_gettime(CLOCK_REALTIME, &ts_frame_begin);

        err = mesh_get_buffer(conn, &buf);
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        printf("INFO: frame_size = %u\n", frame_size);

        if (inputfile == NULL) {
            gen_test_data(buf, frames_processed);
        } else {
            // If we reach the end of the file
            if (read_test_data(inputfile, buf, frame_size) < 0) {
                // If we loop over the file
                if (frame_num == -1) {
                    // reset file
                    fseek(inputfile, 0, SEEK_SET);
                    // read again, if fail, then file does not hold enought data
                    if (read_test_data(inputfile, buf, frame_size) < 0) {
                        break;
                    }
                } else {
                    // if we do not loop over, then we are done
                    break;
                }
            }
        }

        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        if (frames_processed % stat_interval == 0) {
            /* calculate FPS */
            clock_gettime(CLOCK_REALTIME, &ts_end);

            stat_period_s = (ts_end.tv_sec - ts_begin.tv_sec);
            stat_period_s += (ts_end.tv_nsec - ts_begin.tv_nsec) / 1e9;
            fps = stat_interval / stat_period_s;
            throughput_MB = fps * frame_size / 1000000;

            clock_gettime(CLOCK_REALTIME, &ts_begin);
        }

        printf("TX frames: [%d], FPS: %0.2f \n", frames_processed, fps);
        printf("Throughput: %.2lf MB/s, %.2lf Gb/s \n", throughput_MB, throughput_MB * 8 / 1000);

        frames_processed++;

        if (frame_num > 0 && frames_processed >= frame_num) {
            break;
        }

        /* Timestamp for frame end. */
        clock_gettime(CLOCK_REALTIME, &ts_frame_end);

        /* sleep for 1/fps */
        __useconds_t spend = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) +
                             (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec) / 1000;
        printf("spend: %d\n", spend);

        printf("\n");
    }

    sleep(2);

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
    if (inputfile) {
        fclose(inputfile);
    }

    return err;
}
