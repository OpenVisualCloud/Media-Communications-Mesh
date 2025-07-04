/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "mcm.h"
#include "mesh_dp.h"
#include "misc.h"
#include "input.h"

#define SECOND_IN_US (double)1000000.0
#define BLOB_DELAY_IN_US (__useconds_t)1000 // 1ms
/* PRIVATE */
void buffer_to_file(FILE *file, MeshBuffer *buf);

int mcm_send_video_frames(MeshConnection *connection, const char *filename,
                          const char *json_conn_config) {
    video_params video_cfg;
    int err = 0;
    err = get_video_params(json_conn_config, &video_cfg);
    if (err) {
        LOG("[TX] Failed to get video params");
        return err;
    }

    LOG("[TX] Video configuration: %dx%d @ %.2f fps", video_cfg.width, video_cfg.height,
        video_cfg.fps);
    MeshBuffer *buf;
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        LOG("[TX] Failed to serialize video: file is null");
        err = 1;
        return err;
    }

    unsigned int frame_num = 0;
    size_t read_size = 1;
    int sleep_us = (uint32_t)(SECOND_IN_US / video_cfg.fps);
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
        LOG("[TX] Sending frame: %d", ++frame_num);
        err = mesh_put_buffer(&buf);
        if (err) {
            LOG("[TX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
            goto close_file;
        }
        if (shutdown_flag != 0) {
            LOG("[TX] Graceful shutdown requested");
            goto close_file;
        }
        clock_gettime(CLOCK_REALTIME, &ts_frame_end);
        elapsed = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) +
                  (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec) / 1000;
        if (sleep_us > elapsed) {
            usleep(sleep_us - elapsed);
            LOG("[TX] Elapsed: %li; Slept: %li", elapsed, sleep_us - elapsed);
        } else {
            LOG("[TX] Cannot keep the pace with %d fps!", video_cfg.fps);
        }
    }
    LOG("[TX] data sent successfully");
close_file:
    fclose(file);
    return err;
}

int mcm_send_audio_packets(MeshConnection *connection, const char *filename,
                           const char *json_conn_config) {
    audio_params audio_cfg;
    int err = 0;
    err = get_audio_params(json_conn_config, &audio_cfg);
    if (err) {
        LOG("[TX] Failed to get audio params");
        return err;
    }
    LOG("[TX] Audio configuration: channels: %d sample_rate: %li packet_time: %li",
        audio_cfg.channels, audio_cfg.sample_rate, audio_cfg.packet_time);
    MeshBuffer *buf;
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        LOG("[TX] Failed to serialize audio: file is null");
        err = 1;
        return err;
    }
    unsigned int frame_num = 0;
    size_t read_size = 1;
    __useconds_t sleep_us = audio_cfg.packet_time;
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
        if (shutdown_flag != 0) {
            LOG("[TX] Graceful shutdown requested");
            goto close_file;
        }
        clock_gettime(CLOCK_REALTIME, &ts_frame_end);
        elapsed = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) +
                  (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec) / 1000;
        if (sleep_us > elapsed) {
            usleep(sleep_us - elapsed);
            LOG("[TX] Elapsed: %li; Slept: %li", elapsed, sleep_us - elapsed);
        } else {
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
        if (shutdown_flag != 0) {
            LOG("[TX] Graceful shutdown requested");
            goto close_file;
        }
        clock_gettime(CLOCK_REALTIME, &ts_frame_end);
        elapsed = 1000000 * (ts_frame_end.tv_sec - ts_frame_begin.tv_sec) +
                  (ts_frame_end.tv_nsec - ts_frame_begin.tv_nsec) / 1000;
        if (BLOB_DELAY_IN_US > elapsed) {
            usleep(BLOB_DELAY_IN_US - elapsed);
            LOG("[TX] Elapsed: %d; Slept: %d", elapsed, BLOB_DELAY_IN_US - elapsed);
        } else {
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
    FILE *out = NULL;
    
    /* Timer variables for 3-second file rotation */
    struct timespec last_save_time, current_time;
    const long SAVE_INTERVAL_SEC = 3;
    int file_counter = 0;
    char timestamped_filename[512];
    
    /* Initialize timer */
    clock_gettime(CLOCK_REALTIME, &last_save_time);
    
    /* Create initial timestamped filename */
    snprintf(timestamped_filename, sizeof(timestamped_filename), "%s_%d_%ld.dat", 
             filename, file_counter++, last_save_time.tv_sec);
    out = fopen(timestamped_filename, "a");
    if (out == NULL) {
        LOG("[RX] Failed to open initial output file: %s", timestamped_filename);
        return;
    }
    LOG("[RX] Created initial output file: %s", timestamped_filename);
    
    while (1) {
        /* Check if 3 seconds have elapsed since last file save */
        clock_gettime(CLOCK_REALTIME, &current_time);
        if ((current_time.tv_sec - last_save_time.tv_sec) >= SAVE_INTERVAL_SEC) {
            /* Close current file */
            if (out != NULL) {
                fclose(out);
                LOG("[RX] Closed file: %s", timestamped_filename);
            }
            
            /* Create new timestamped filename */
            snprintf(timestamped_filename, sizeof(timestamped_filename), "%s_%d_%ld.dat", 
                     filename, file_counter++, current_time.tv_sec);
            
            /* Open new file */
            out = fopen(timestamped_filename, "a");
            if (out == NULL) {
                LOG("[RX] Failed to open new output file: %s", timestamped_filename);
                break;
            }
            LOG("[RX] Created new output file: %s", timestamped_filename);
            
            /* Update last save time */
            last_save_time = current_time;
        }

        /* Set loop's error */
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
    
    if (out != NULL) {
        fclose(out);
        LOG("[RX] Closed final output file: %s", timestamped_filename);
    }
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

void read_data_in_loop_with_interval(MeshConnection *connection, const char *filename, int save_interval_sec) {
    int timeout = MESH_TIMEOUT_INFINITE;
    int frame = 0;
    int err = 0;
    MeshBuffer *buf = NULL;
    FILE *out = NULL;
    
    /* Timer variables for configurable file rotation */
    struct timespec last_save_time, current_time;
    int file_counter = 0;
    char timestamped_filename[512];
    
    /* Initialize timer */
    clock_gettime(CLOCK_REALTIME, &last_save_time);
    
    /* Create initial timestamped filename with more detailed timestamp */
    struct tm *tm_info = localtime(&last_save_time.tv_sec);
    snprintf(timestamped_filename, sizeof(timestamped_filename), 
             "%s_%04d%02d%02d_%02d%02d%02d_%d.dat", 
             filename, 
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             file_counter++);
    
    out = fopen(timestamped_filename, "a");
    if (out == NULL) {
        LOG("[RX] Failed to open initial output file: %s", timestamped_filename);
        return;
    }
    LOG("[RX] Created initial output file: %s (saving every %d seconds)", timestamped_filename, save_interval_sec);
    
    while (1) {
        /* Check if configured interval has elapsed since last file save */
        clock_gettime(CLOCK_REALTIME, &current_time);
        if ((current_time.tv_sec - last_save_time.tv_sec) >= save_interval_sec) {
            /* Close current file and flush any remaining data */
            if (out != NULL) {
                fflush(out);
                fclose(out);
                LOG("[RX] Closed file: %s", timestamped_filename);
            }
            
            /* Create new timestamped filename */
            tm_info = localtime(&current_time.tv_sec);
            snprintf(timestamped_filename, sizeof(timestamped_filename), 
                     "%s_%04d%02d%02d_%02d%02d%02d_%d.dat", 
                     filename, 
                     tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                     tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
                     file_counter++);
            
            /* Open new file */
            out = fopen(timestamped_filename, "a");
            if (out == NULL) {
                LOG("[RX] Failed to open new output file: %s", timestamped_filename);
                break;
            }
            LOG("[RX] Created new output file: %s", timestamped_filename);
            
            /* Update last save time */
            last_save_time = current_time;
        }

        /* Set loop's error */
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
        
        /* Flush data to ensure it's written immediately */
        if (out != NULL) {
            fflush(out);
        }

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
    
    if (out != NULL) {
        fflush(out);
        fclose(out);
        LOG("[RX] Closed final output file: %s", timestamped_filename);
    }
    LOG("[RX] Done reading the data");
}

int is_root() { return geteuid() == 0; }
