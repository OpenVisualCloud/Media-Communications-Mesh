/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MTL_H
#define __MTL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#include <mtl/st30_api.h> /* st2110-30 */
#include <mtl/st40_api.h> /* st2110-40 */
#include <mtl/st_pipeline_api.h> /* st2110 pipeline */
#include <errno.h>

#include <mcm_dp.h>

#include "app_platform.h"
#include "shm_memif.h" /* share memory */
#include "utils.h"

#ifndef NS_PER_S
#define NS_PER_S (1000000000)
#endif

#ifndef NS_PER_US
#define NS_PER_US (1000)
#endif

#ifndef NS_PER_MS
#define NS_PER_MS (1000 * 1000)
#endif

typedef struct {
    mtl_handle st;
    int idx;
    st20p_rx_handle handle;

    bool stop;
    pthread_t frame_thread;

    int fb_recv;
    pthread_cond_t wake_cond;
    pthread_mutex_t wake_mutex;

    size_t frame_size; /* Size (Bytes) of single frame. */
    uint32_t fb_count; /* Frame buffer count. */

    uint32_t width;
    uint32_t height;
    enum st_frame_fmt output_fmt;

#if defined(ZERO_COPY)
    uint8_t* source_begin;
    uint8_t* source_end;
    uint8_t* frame_cursor;

    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;
#endif

    /* share memory arguments */
    memif_socket_args_t memif_socket_args;
    memif_conn_args_t memif_conn_args;

    /* memif conenction handle */
    memif_socket_handle_t memif_socket;
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    pthread_t memif_event_thread;

    void* frames_malloc_addr;
    void* frames_begin_addr;
    mtl_iova_t frames_begin_iova;
    size_t frames_iova_map_sz;
    struct st20_ext_frame* ext_frames;
} rx_session_context_t;

typedef struct {
    mtl_handle st;
    int idx;
    st20p_tx_handle handle;

    bool stop;

    int fb_send;
    pthread_cond_t wake_cond;
    pthread_mutex_t wake_mutex;

    size_t frame_size;

#if defined(ZERO_COPY)
    uint8_t* source_begin;
    uint8_t* source_end;
    uint8_t* frame_cursor;

    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;
#endif

    /* memif parameters */
    memif_ops_t memif_ops;
    /* share memory arguments */
    memif_conn_args_t memif_conn_args;
    /* memif conenction handle */
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    memif_socket_args_t memif_socket_args;
    memif_socket_handle_t memif_socket;
    pthread_t memif_event_thread;
} tx_session_context_t;

typedef struct {
    mtl_handle st;
    int idx;
    st22p_tx_handle handle;

    bool stop;

    int fb_cnt;
    uint16_t fb_idx;
    int fb_send;
    pthread_cond_t st22p_wake_cond;
    pthread_mutex_t st22p_wake_mutex;

    size_t frame_size;

#if defined(ZERO_COPY)
    uint8_t* source_begin;
    uint8_t* source_end;
    uint8_t* frame_cursor;

    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;

    void* ext_fb_malloc;
    uint8_t* ext_fb;
    mtl_iova_t ext_fb_iova;
    size_t ext_fb_iova_map_sz;
    struct st_ext_frame* p_ext_frames;
    int ext_idx;
    bool ext_fb_in_use[3]; /* assume 3 framebuffer */
    mtl_dma_mem_handle dma_mem;
#endif

    /* memif parameters */
    memif_ops_t memif_ops;
    /* share memory arguments */
    memif_conn_args_t memif_conn_args;
    /* memif conenction handle */
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    memif_socket_args_t memif_socket_args;
    memif_socket_handle_t memif_socket;
    pthread_t memif_event_thread;
} tx_st22p_session_context_t;

typedef struct {
    mtl_handle st;
    int idx;
    st22p_rx_handle handle;

    bool stop;
    pthread_t frame_thread;

    int fb_recv;
    pthread_cond_t st22p_wake_cond;
    pthread_mutex_t st22p_wake_mutex;

    size_t frame_size; /* Size (Bytes) of single frame. */
    uint16_t fb_idx;
    uint32_t fb_count; /* Frame buffer count. */

    uint32_t width;
    uint32_t height;
    enum st_frame_fmt output_fmt;

#if defined(ZERO_COPY)
    uint8_t* source_begin;
    uint8_t* source_end;
    uint8_t* frame_cursor;

    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;

    void* ext_fb_malloc;
    uint8_t* ext_fb;
    mtl_iova_t ext_fb_iova;
    size_t ext_fb_iova_map_sz;
    struct st20_ext_frame* ext_frames;
    struct st_ext_frame* p_ext_frames;
    int ext_idx;
    bool ext_fb_in_use[3]; /* assume 3 framebuffer */
    mtl_dma_mem_handle dma_mem;
#endif

    /* share memory arguments */
    memif_socket_args_t memif_socket_args;
    memif_conn_args_t memif_conn_args;

    /* memif conenction handle */
    memif_socket_handle_t memif_socket;
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    pthread_t memif_event_thread;

    void* frames_malloc_addr;
    void* frames_begin_addr;
    mtl_iova_t frames_begin_iova;
    size_t frames_iova_map_sz;
} rx_st22p_session_context_t;

typedef struct {
    mtl_handle st;
    int idx;
    st30_tx_handle handle;

    uint16_t framebuff_cnt;
    uint16_t framebuff_producer_idx;
    uint16_t framebuff_consumer_idx;
    struct st_tx_frame* framebuffs;

    int st30_frame_done_cnt;
    int st30_packet_done_cnt;

    enum st30_sampling sampling;

    bool stop;

    int fb_send;
    pthread_cond_t st30_wake_cond;
    pthread_mutex_t st30_wake_mutex;

    int st30_frame_size;
    size_t pkt_len;

    uint32_t fb_count; /* Frame buffer count. */

    /* memif parameters */
    memif_ops_t memif_ops;
    /* share memory arguments */
    memif_conn_args_t memif_conn_args;
    /* memif conenction handle */
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    memif_socket_args_t memif_socket_args;
    memif_socket_handle_t memif_socket;
    pthread_t memif_event_thread;
} tx_st30_session_context_t;

typedef struct {
    mtl_handle st;
    int idx;
    st30_rx_handle handle;

    bool stop;
    pthread_t frame_thread;

    int fb_recv;

    pthread_t st30_app_thread;
    pthread_cond_t st30_wake_cond;
    pthread_mutex_t st30_wake_mutex;
    bool st30_app_thread_stop;

    int st30_frame_size;
    int pkt_len;

    uint32_t fb_count; /* Frame buffer count. */

    /* share memory arguments */
    memif_socket_args_t memif_socket_args;
    memif_conn_args_t memif_conn_args;

    /* memif conenction handle */
    memif_socket_handle_t memif_socket;
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    pthread_t memif_event_thread;

    /* stat */
    int stat_frame_total_received;
    uint64_t stat_frame_first_rx_time;
    double expect_fps;
} rx_st30_session_context_t;

typedef struct {
    mtl_handle st;
    int idx;
    st40_tx_handle handle;

    uint16_t framebuff_cnt;
    uint16_t framebuff_producer_idx;
    uint16_t framebuff_consumer_idx;
    struct st_tx_frame* framebuffs;

    int st40_frame_done_cnt;
    int st40_packet_done_cnt;

    bool stop;

    int fb_send;
    pthread_cond_t st40_wake_cond;
    pthread_mutex_t st40_wake_mutex;

    int st40_frame_size;
    size_t pkt_len;

    uint32_t fb_count; /* Frame buffer count. */

    /* memif parameters */
    memif_ops_t memif_ops;
    /* share memory arguments */
    memif_conn_args_t memif_conn_args;
    /* memif conenction handle */
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    memif_socket_args_t memif_socket_args;
    memif_socket_handle_t memif_socket;
    pthread_t memif_event_thread;
} tx_st40_session_context_t;

typedef struct {
    mtl_handle st;
    int idx;
    st40_rx_handle handle;

    bool stop;
    pthread_t frame_thread;

    int fb_recv;

    pthread_t st40_app_thread;
    pthread_cond_t st40_wake_cond;
    pthread_mutex_t st40_wake_mutex;

    int st40_frame_size;
    int pkt_len;

    uint32_t fb_count; /* Frame buffer count. */

    /* share memory arguments */
    memif_socket_args_t memif_socket_args;
    memif_conn_args_t memif_conn_args;

    /* memif conenction handle */
    memif_socket_handle_t memif_socket;
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    pthread_t memif_event_thread;

    /* stat */
    int stat_frame_total_received;
    uint64_t stat_frame_first_rx_time;
    double expect_fps;
} rx_st40_session_context_t;

/* Initialize MTL library */
mtl_handle inst_init(struct mtl_init_params* st_param);

/* Deinitialize MTL */
void mtl_deinit(mtl_handle dev_handle);

/* TX: Create ST20P session */
tx_session_context_t* mtl_st20p_tx_session_create(mtl_handle dev_handle, struct st20p_tx_ops* opts, memif_ops_t* memif_ops);

/* RX: Create ST20P session */
rx_session_context_t* mtl_st20p_rx_session_create(mtl_handle dev_handle, struct st20p_rx_ops* opts, memif_ops_t* memif_ops);

/* TX: Stop ST20P session */
void mtl_st20p_tx_session_stop(tx_session_context_t* tx_ctx);

/* RX: Stop ST20P session */
void mtl_st20p_rx_session_stop(rx_session_context_t* rx_ctx);

/* TX: Destroy ST20P session */
void mtl_st20p_tx_session_destroy(tx_session_context_t** p_tx_ctx);

/* RX: Destroy ST20P session */
void mtl_st20p_rx_session_destroy(rx_session_context_t** p_rx_ctx);

/* TX: Create ST22P session */
tx_st22p_session_context_t* mtl_st22p_tx_session_create(mtl_handle dev_handle, struct st22p_tx_ops* opts, memif_ops_t* memif_ops);

/* RX: Create ST22P session */
rx_st22p_session_context_t* mtl_st22p_rx_session_create(mtl_handle dev_handle, struct st22p_rx_ops* opts, memif_ops_t* memif_ops);

/* TX: Create ST30 session */
tx_st30_session_context_t* mtl_st30_tx_session_create(mtl_handle dev_handle, struct st30_tx_ops* opts, memif_ops_t* memif_ops);

/* RX: Create ST30 session */
rx_st30_session_context_t* mtl_st30_rx_session_create(mtl_handle dev_handle, struct st30_rx_ops* opts, memif_ops_t* memif_ops);

/* TX: Create ST40 session */
tx_st40_session_context_t* mtl_st40_tx_session_create(mtl_handle dev_handle, struct st40_tx_ops* opts, memif_ops_t* memif_ops);

/* RX: Create ST40 session */
rx_st40_session_context_t* mtl_st40_rx_session_create(mtl_handle dev_handle, struct st40_rx_ops* opts, memif_ops_t* memif_ops);

/* TX: Stop ST22P session */
void mtl_st22p_tx_session_stop(tx_st22p_session_context_t* tx_ctx);

/* RX: Stop ST22P session */
void mtl_st22p_rx_session_stop(rx_st22p_session_context_t* rx_ctx);

/* TX: Destroy ST22P session */
void mtl_st22p_tx_session_destroy(tx_st22p_session_context_t** p_tx_ctx);

/* RX: Destroy ST22P session */
void mtl_st22p_rx_session_destroy(rx_st22p_session_context_t** p_rx_ctx);

/* TX: Stop ST30 session */
void mtl_st30_tx_session_stop(tx_st30_session_context_t* pctx);

/* RX: Stop ST30 session */
void mtl_st30_rx_session_stop(rx_st30_session_context_t* pctx);

/* TX: Destroy ST30 session */
void mtl_st30_tx_session_destroy(tx_st30_session_context_t** ppctx);

/* RX: Destroy ST30 session */
void mtl_st30_rx_session_destroy(rx_st30_session_context_t** ppctx);

/* TX: Stop ST40 session */
void mtl_st40_tx_session_stop(tx_st40_session_context_t* pctx);

/* RX: Stop ST40 session */
void mtl_st40_rx_session_stop(rx_st40_session_context_t* pctx);

/* TX: Destroy ST40 session */
void mtl_st40_tx_session_destroy(tx_st40_session_context_t** ppctx);

/* RX: Destroy ST40 session */
void mtl_st40_rx_session_destroy(rx_st40_session_context_t** ppctx);

#ifdef __cplusplus
}
#endif

#endif /* __MTL_H */
