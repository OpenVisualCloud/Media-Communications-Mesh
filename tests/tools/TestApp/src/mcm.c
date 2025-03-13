/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mcm.h"
#include "mesh_dp.h"
#include "misc.h"
#include "json_context.h"

#define SECOND_IN_US (double)1000000.0
/* PRIVATE */
void buffer_to_file(FILE *file, MeshBuffer *buf);

int mcm_send_video_frames(MeshConnection *connection, const char *filename) {
    MeshConfig_Video video_cfg = get_video_params(connection);
    LOG("[TX] Video configuration: %dx%d @ %.2f fps", video_cfg.width, video_cfg.height, video_cfg.fps);
    LOG("[TX] Video pixel format: %d", video_cfg.pixel_format);
    int err = 0;
    MeshBuffer *buf;
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        LOG("[TX] Failed to serialize video: file is null");
        err = 1;
        return err;
    }
    
    /* execute cpp class code  here */
    unsigned int frame_num = 0;
    size_t read_size = 1;
    float one_sec = 1000000.0; /* in microseconds (usec) */
    float requested_fps = 25.0; /* FIXME: Read requested fps value */
    float frame_sleep_time = one_sec/requested_fps;
    LOG("[TX] Requested fps: %.2f | Frame sleep time: %.2f", requested_fps, frame_sleep_time);
    int sleep_us = (uint32_t)(SECOND_IN_US/video_cfg.fps);
    while (1) {

        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(connection, &buf);
        if (err) {
            LOG("[TX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        read_size = fread(buf->payload_ptr, 1, buf->payload_len, file);
        if (read_size == 0) {
            goto close_file;
        }

        /* Send the buffer */
        LOG("[TX] Sending frame: %d", ++frame_num);
        err = mesh_put_buffer(&buf);
        if (err) {
            LOG("[TX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        usleep(sleep_us);
    }
    LOG("[TX] data sent successfully");
close_file:
    fclose(file);
    return err;
}

void read_data_in_loop(MeshConnection *connection, const char *filename) {
    int timeout = MESH_TIMEOUT_INFINITE;
    int frame = 0;
    int err = 0;
    MeshBuffer *buf = NULL;
    FILE *out = fopen(filename, "a");
    while (1) {

        /* Set loop's  error*/
        err = 0;
        if (frame) {
            timeout = 1000;
        }

        /* Receive a buffer from the mesh */
        err = mesh_get_buffer_timeout(connection, &buf, timeout);
        if (err == MESH_ERR_CONN_CLOSED) {
            LOG("[RX] Connection closed");
            break;
        }
        LOG("[RX] Fetched mesh data buffer");
        if (err) {
            LOG("[RX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
            break;
        }
        /* Process the received user data */
        buffer_to_file(out, buf);

        err = mesh_put_buffer(&buf);
        if (err) {
            LOG("[RX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
            break;
        }
        LOG("[RX] Frame: %d", ++frame);
    }
    fclose(out);
    LOG("[RX] Done reading the data");
}

void buffer_to_file(FILE *file, MeshBuffer *buf) {
    if (file == NULL) {
        LOG("[RX] Failed to open file for writing");
        return;
    }
    // Write the buffer to the file
    fwrite(buf->payload_ptr, buf->payload_len, 1, file);
    LOG("[RX] Saving buffer data to a file");
}

int is_root() { return geteuid() == 0; }

