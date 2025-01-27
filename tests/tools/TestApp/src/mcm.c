/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mcm.h"
#include "mesh_dp.h"

/* PRIVATE */
void buffer_to_file(FILE *file, MeshBuffer *buf);

int mcm_send_video_frames(MeshConnection *connection, MeshClient *client, FILE *file) {
    int err = 0;
    MeshBuffer *buf;
    if (file == NULL) {
        printf("[TX] Failed to serialize video: file is null \n");
        exit(1);
    }

    unsigned char frame_num = 0;
    size_t read_size = 1;
    while (1) {

        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(connection, &buf);
        if (err) {
            printf("[TX] Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            exit(err);
        }
        read_size = fread(buf->payload_ptr, 1, buf->payload_len, file);
        if (read_size == 0) {
            exit(2);
        }

        /* Send the buffer */
        printf("[TX] Sending frame: %d\n", ++frame_num);
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("[TX] Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            exit(err);
        }

        /* Temporary implementation for pacing */
        /* TODO: Implement pacing calculation */
        usleep(40000);
    }
    printf("[TX] data sent successfully \n");
    return err;
}

void read_data_in_loop(MeshConnection *connection, const char *filename) {
    int timeout = MESH_TIMEOUT_INFINITE;
    int frame = 0;
    int err = 0;
    MeshBuffer *buf = NULL;
    while (1) {
        FILE *out = fopen(filename, "a");
        /* Set loop's  error*/
        err = 0;
        if (frame) {
            timeout = 1000;
        }

        /* Receive a buffer from the mesh */
        err = mesh_get_buffer_timeout(connection, &buf, timeout);
        if (err == MESH_ERR_CONN_CLOSED) {
            printf("[RX] Connection closed\n");
            exit(err);
        }
        printf("[RX] Fetched mesh data buffer\n");
        if (err) {
            printf("[RX] Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            exit(err);
        }
        /* Process the received user data */
        buffer_to_file(out, buf);

        err = mesh_put_buffer(&buf);
        if (err) {
            printf("[RX] Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            exit(err);
        }
        printf("[RX] Frame: %d\n", ++frame);
        fclose(out);
    }
    printf("[RX] Done reading the data \n");
}

void buffer_to_file(FILE *file, MeshBuffer *buf) {
    if (file == NULL) {
        perror("[RX] Failed to open file for writing");
        return;
    }
    // Write the buffer to the file
    unsigned int *temp_buf = buf->payload_ptr;
    fwrite(buf->payload_ptr, buf->payload_len, 1, file);
    printf("[RX] Saving buffer data to a file\n");
}

int is_root() { return (geteuid() == 0) ? 1 : 0; }