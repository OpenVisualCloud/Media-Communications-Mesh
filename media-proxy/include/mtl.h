/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
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

#include <mcm_dp.h>

#include "app_platform.h"
#include "shm_memif.h" /* share memory */
#include "utils.h"
/*used by UDP H264*/
#include <mtl/mudp_api.h>
#include <mtl/mtl_sch_api.h>

#ifndef NS_PER_S
#define NS_PER_S (1000000000)
#endif

#ifndef NS_PER_US
#define NS_PER_US (1000)
#endif

#ifndef NS_PER_MS
#define NS_PER_MS (1000 * 1000)
#endif

#define SCH_CNT 1
#define TASKLETS 100

enum direction {
    TX,
    RX
};

typedef struct {
    uint8_t is_master;
    char app_name[32];
    char interface_name[32];
    uint32_t interface_id;
    char socket_path[108];
} memif_ops_t;

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

enum sample_udp_mode {
    SAMPLE_UDP_TRANSPORT_H264,
};

typedef struct {
    mtl_handle st;
    struct mtl_init_params param;
    uint8_t rx_sip_addr[MTL_PORT_MAX][MTL_IP_ADDR_LEN]; /* rx source IP */
    uint16_t framebuff_cnt;
    uint16_t udp_port;
    uint8_t payload_type;
    // uint32_t sessions; /* number of sessions */
    bool ext_frame;
    bool hdr_split;
    bool rx_dump;
    uint32_t fb_count; /* Frame buffer count. */
    enum sample_udp_mode udp_mode;
    uint64_t udp_tx_bps;
    int udp_len;
    bool exit;
    bool has_user_meta; /* if provide user meta data with the st2110-20 frame */
    pthread_t thread;
    pthread_cond_t wake_cond;
    pthread_mutex_t wake_mutex;
    mudp_handle socket;
    struct sockaddr_in client_addr;
    struct sockaddr_in bind_addr;
    bool stop;

    /* share memory arguments */
    memif_socket_args_t memif_socket_args;
    memif_conn_args_t memif_conn_args;

    /* memif conenction handle */
    memif_socket_handle_t memif_socket;
    memif_conn_handle_t memif_conn;

    memif_buffer_t* shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    uint32_t memif_nalu_size;

    char name[32];
    pthread_t memif_event_thread;

    /*udp poll*/
    //mtl_sch_handle schs[SCH_CNT];
    mtl_tasklet_handle udp_tasklet;
    struct mtl_tasklet_ops udp_tasklet_ops;
    struct mudp_pollfd udp_pollfd;
    bool sch_start;
    int new_NALU;
    bool check_first_new_NALU;
    //int sch_idx;
    //int tasklet_idx;

} rx_udp_h264_session_context_t;

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

typedef struct {
    uint32_t id;
    enum direction type;
    mcm_payload_type payload_type;
    union {
        tx_session_context_t* tx_session;
        rx_session_context_t* rx_session;
        tx_st22p_session_context_t* tx_st22p_session;
        rx_st22p_session_context_t* rx_st22p_session;
        tx_st30_session_context_t* tx_st30_session;
        rx_st30_session_context_t* rx_st30_session;
        rx_udp_h264_session_context_t* rx_udp_h264_session;
        tx_st40_session_context_t* tx_st40_session;
        rx_st40_session_context_t* rx_st40_session;
    };
} mtl_session_context_t;

/* Initialize IMTL library */
mtl_handle inst_init(struct mtl_init_params* st_param);

/* Deinitialize IMTL */
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

/* RX: Stop RTSP session */
void mtl_rtsp_rx_session_stop(rx_udp_h264_session_context_t* rx_ctx);

/* TX: Destroy ST22P session */
void mtl_st22p_tx_session_destroy(tx_st22p_session_context_t** p_tx_ctx);

/* RX: Destroy ST22P session */
void mtl_st22p_rx_session_destroy(rx_st22p_session_context_t** p_rx_ctx);

/* RX: Destroy RTSP session */
void mtl_rtsp_rx_session_destroy(rx_udp_h264_session_context_t** p_rx_ctx);

/* RX: Create UDP H264 session */
rx_udp_h264_session_context_t* mtl_udp_h264_rx_session_create(mtl_handle dev_handle, mcm_dp_addr* dp_addr, memif_ops_t* memif_ops, mtl_sch_handle schs[]);

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
