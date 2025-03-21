/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "mcm.h"
#include "mesh_dp.h"
#include "misc.h"
#include "json_context.h"
#include <errno.h>

#define SECOND_IN_US (double)1000000.0
#define BLOB_DELAY_IN_US (__useconds_t)1000 //1ms
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
    int sleep_us = (uint32_t)(SECOND_IN_US/video_cfg.fps);
    struct timespec ts_begin = {}, ts_end = {};
    struct timespec ts_frame_begin = {}, ts_frame_end = {};
    __useconds_t elapsed = 0;
    while (1) {
        clock_gettime(CLOCK_REALTIME, &ts_frame_begin);
        LOG("[TX] Sending frame: %d", ++frame_num);

        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(connection, &buf);
        if (err) {
            LOG("[TX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        read_size = fread(buf->payload_ptr, 1, buf->payload_len, file);
        if (read_size == 0) {
            mesh_buffer_set_payload_len(buf, 0);
            mesh_put_buffer(&buf);
            if (err) {
                LOG("[TX] Failed to set buffer_len: %s (%d)", mesh_err2str(err), err);
            }
            goto close_file;
        }
        err = mesh_put_buffer(&buf);
        if (err) {
            LOG("[TX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        if (shutdown_flag  != 0 ) {
            LOG("[TX] Graceful shutdown requested");
            goto close_file;
        }
        clock_gettime(CLOCK_REALTIME, &ts_frame_end);
        elapsed = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) + (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec)/1000;
        if (sleep_us - elapsed >= 0) {
            usleep(sleep_us - elapsed);
            LOG("[TX] Elapsed: %d; Slept: %d", elapsed, sleep_us - elapsed);
        }
        else {
            LOG("[TX] Cannot keep the pace with %d fps!", video_cfg.fps);
        }
    }
    LOG("[TX] data sent successfully");
close_file:
    fclose(file);
    return err;
}

int mcm_send_audio_packets(MeshConnection *connection, const char *filename) {
    /* 
        packet_time, format, sample_rate match tables,
        order as in Media-Communications-Mesh/sdk/include/mesh_dp.hL231
    */
    int packet_time_convert_table_us[] = {1000, 125, 250, 333, 4000, 80, 1009, 140, 90};
    const char* format_convert_table_str[] = {"pcms8", "pcms16be", "pcms24be"};
    int sample_rate_convert_table_hz[] = {48000, 96000, 44100};
    MeshConfig_Audio audio_cfg = get_audio_params(connection);
    LOG("[TX] Audio configuration: channels: %d sample_rate: %d packet_time: %d", audio_cfg.channels, 
        sample_rate_convert_table_hz[audio_cfg.sample_rate], packet_time_convert_table_us[audio_cfg.packet_time]);
    LOG("[TX] Audio format: %s", format_convert_table_str[audio_cfg.format]);
    int err = 0;
    MeshBuffer *buf;
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        LOG("[TX] Failed to serialize audio: file is null");
        err = 1;
        return err;
    }
    unsigned int frame_num = 0;
    size_t read_size = 1;
    __useconds_t sleep_us = packet_time_convert_table_us[audio_cfg.packet_time];
    struct timespec ts_begin = {}, ts_end = {};
    struct timespec ts_frame_begin = {}, ts_frame_end = {};
    __useconds_t elapsed = 0;
    while (1) {
        clock_gettime(CLOCK_REALTIME, &ts_frame_begin);

        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(connection, &buf);
        if (err) {
            LOG("[TX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        read_size = fread(buf->payload_ptr, 1, buf->payload_len, file);
        if (read_size == 0) {
            mesh_buffer_set_payload_len(buf, 0);
            mesh_put_buffer(&buf);
            if (err) {
                LOG("[TX] Failed to set buffer_len: %s (%d)", mesh_err2str(err), err);
            }
            goto close_file;
        }
        if (read_size > buf->payload_len) {
            LOG("[TX] read_size is bigger than payload_len: %s (%d)", mesh_err2str(err), err);
            mesh_buffer_set_payload_len(buf, 0);
            mesh_put_buffer(&buf);
            if (err) {
                LOG("[TX] Failed to set buffer_len: %s (%d)", mesh_err2str(err), err);
            }
            goto close_file;
        }
        err = mesh_buffer_set_payload_len(buf, read_size);
        if (err) {
            LOG("[TX] Failed to set buffer_len: %s (%d)", mesh_err2str(err), err);
        }
        LOG("[TX] Sending packet: %d", ++frame_num);
        err = mesh_put_buffer(&buf);
        if (err) {
            LOG("[TX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        if (shutdown_flag  != 0 ) {
            LOG("[TX] Graceful shutdown requested");
            goto close_file;
        }
        clock_gettime(CLOCK_REALTIME, &ts_frame_end);
        elapsed = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) + (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec)/1000;
        if (sleep_us > elapsed) {
            usleep(sleep_us - elapsed);
            LOG("[TX] Elapsed: %d; Slept: %d", elapsed, sleep_us - elapsed);
        }
        else {
            LOG("[TX] Cannot keep the pace with %d time!", sleep_us);
        }
    }
    LOG("[TX] data sent successfully");
close_file:
    fclose(file);
    return err;
}

int mcm_send_blob_packets(MeshConnection *connection, const char *filename) {
    LOG("[TX] sending blob packets");
    int err = 0;
    MeshBuffer *buf;
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        LOG("[TX] Sending blob packets. Failed to open file %s: %s", filename, strerror(errno));
        err = 1;
        return err;
    }
    
    /* execute cpp class code  here */
    unsigned int frame_num = 0;
    size_t read_size = 1;
    struct timespec ts_begin = {}, ts_end = {};
    struct timespec ts_frame_begin = {}, ts_frame_end = {};
    __useconds_t elapsed = 0;
    while (1) {
        clock_gettime(CLOCK_REALTIME, &ts_frame_begin);
        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(connection, &buf);
        if (err) {
            LOG("[TX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        read_size = fread(buf->payload_ptr, 1, buf->payload_len, file);
        if (read_size == 0) {
            mesh_buffer_set_payload_len(buf, 0);
            mesh_put_buffer(&buf);
            if (err) {
                LOG("[TX] Failed to set buffer_len: %s (%d)", mesh_err2str(err), err);
            }
            goto close_file;
        }
        
        if (read_size > buf->payload_len) {
            LOG("[TX] read_size is bigger than payload_len: %s (%d)", mesh_err2str(err), err);
            mesh_buffer_set_payload_len(buf, 0);
            mesh_put_buffer(&buf);
            if (err) {
                LOG("[TX] Failed to set buffer_len: %s (%d)", mesh_err2str(err), err);
            }
            goto close_file;
        }
        err = mesh_buffer_set_payload_len(buf, read_size);
        if (err) {
            LOG("[TX] Failed to set buffer_len: %s (%d)", mesh_err2str(err), err);
        }
        LOG("[TX] Sending packet: %d", ++frame_num);
        err = mesh_put_buffer(&buf);
        if (err) {
            LOG("[TX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        if (shutdown_flag  != 0 ) {
            LOG("[TX] Graceful shutdown requested");
            goto close_file;
        }
        clock_gettime(CLOCK_REALTIME, &ts_frame_end);
        elapsed = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) + (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec)/1000;
        if (BLOB_DELAY_IN_US > elapsed) {
            usleep(BLOB_DELAY_IN_US - elapsed);
            LOG("[TX] Elapsed: %d; Slept: %d", elapsed, BLOB_DELAY_IN_US - elapsed);
        }
        else {
            LOG("[TX] Cannot keep the pace with %d time, dropping packet!", BLOB_DELAY_IN_US);

        }
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
        timeout = (frame) ? 1000 : MESH_TIMEOUT_INFINITE;

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
        if (shutdown_flag != 0) {
            LOG("[RX] Graceful shutdown requested");
            break;
        }
    }
    fclose(out);
    LOG("[RX] Done reading the data");
}

/* common for video, audio, blob*/
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
