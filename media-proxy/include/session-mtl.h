/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SESSION_MTL_H
#define __SESSION_MTL_H

#include <mtl/st30_api.h>        /* st2110-30 */
#include <mtl/st40_api.h>        /* st2110-40 */
#include <mtl/st_pipeline_api.h> /* st2110 pipeline */

#include "session-base.h"

#define MTL_ZERO_COPY

class mtl_session : public session
{
  protected:
    mtl_handle st;

    pthread_cond_t wake_cond;
    pthread_mutex_t wake_mutex;

    volatile bool stop;

    mtl_session();
    virtual ~mtl_session();

  public:
    int frame_available_cb();
    void session_stop();
};

class rx_st20_mtl_session : public mtl_session
{
    st20p_rx_handle handle;

    std::thread *frame_thread_handle;

    memif_buffer_t *shm_bufs;

    int fb_recv;
    size_t frame_size; /* Size (Bytes) of single frame. */

#if defined(MTL_ZERO_COPY)
  private:
    enum st_frame_fmt output_fmt;
    uint8_t *source_begin;
    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;

    int on_connect_cb(memif_conn_handle_t conn);
    int on_disconnect_cb(memif_conn_handle_t conn);

  public:
    int query_ext_frame_cb(struct st_ext_frame *ext_frame, struct st20_rx_frame_meta *meta);
#endif

  private:
    void consume_frame(struct st_frame *frame);
    void frame_thread();
    void cleanup();

  public:
    rx_st20_mtl_session(mtl_handle dev_handle, struct st20p_rx_ops *opts, memif_ops_t *memif_ops);
    ~rx_st20_mtl_session();
};

class tx_st20_mtl_session : public mtl_session
{
    st20p_tx_handle handle;

    int fb_send;
    size_t frame_size;

#if defined(MTL_ZERO_COPY)
  private:
    uint8_t *source_begin;
    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;

    int on_connect_cb(memif_conn_handle_t conn);
    int on_disconnect_cb(memif_conn_handle_t conn);
#endif

  private:
    int on_receive_cb(memif_conn_handle_t conn, uint16_t qid);
    void cleanup();

  public:
    tx_st20_mtl_session(mtl_handle dev_handle, struct st20p_tx_ops *opts, memif_ops_t *memif_ops);
    ~tx_st20_mtl_session();

    int frame_done_cb(struct st_frame *frame);
};

class rx_st22_mtl_session : public mtl_session
{
    st22p_rx_handle handle;

    std::thread *frame_thread_handle;

    memif_buffer_t *shm_bufs;

    int fb_recv;
    size_t frame_size; /* Size (Bytes) of single frame. */

#if defined(MTL_ZERO_COPY)
  private:
    enum st_frame_fmt output_fmt;
    uint32_t width;
    uint32_t height;
    uint8_t *source_begin;
    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;

    int on_connect_cb(memif_conn_handle_t conn);
    int on_disconnect_cb(memif_conn_handle_t conn);

  public:
    int query_ext_frame_cb(struct st_ext_frame *ext_frame, struct st22_rx_frame_meta *meta);
#endif

  private:
    void consume_frame(struct st_frame *frame);
    void frame_thread();
    void cleanup();

  public:
    rx_st22_mtl_session(mtl_handle dev_handle, struct st22p_rx_ops *opts, memif_ops_t *memif_ops);
    ~rx_st22_mtl_session();
};

class tx_st22_mtl_session : public mtl_session
{
    st22p_tx_handle handle;

    int fb_send;
    size_t frame_size;

#if defined(MTL_ZERO_COPY)
  private:
    uint8_t *source_begin;
    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;

    int on_connect_cb(memif_conn_handle_t conn);
    int on_disconnect_cb(memif_conn_handle_t conn);
#endif

  private:
    int on_receive_cb(memif_conn_handle_t conn, uint16_t qid);
    void cleanup();

  public:
    tx_st22_mtl_session(mtl_handle dev_handle, struct st22p_tx_ops *opts, memif_ops_t *memif_ops);
    ~tx_st22_mtl_session();

    int frame_done_cb(struct st_frame *frame);
};

class rx_st30_mtl_session : public mtl_session
{
    st30_rx_handle handle;

    int fb_recv;
    int pkt_len;

    memif_buffer_t *shm_bufs;

    void cleanup();

  public:
    rx_st30_mtl_session(mtl_handle dev_handle, struct st30_rx_ops *opts, memif_ops_t *memif_ops);
    ~rx_st30_mtl_session();

    int frame_ready_cb(void *frame, struct st30_rx_frame_meta *meta);
};

class tx_st30_mtl_session : public mtl_session
{
    st30_tx_handle handle;
    enum st30_sampling sampling;

    uint16_t framebuff_cnt;
    uint16_t framebuff_producer_idx;
    uint16_t framebuff_consumer_idx;
    struct st_tx_frame *framebuffs;

    int fb_send;
    size_t pkt_len;

    int on_receive_cb(memif_conn_handle_t conn, uint16_t qid);
    void cleanup();

  public:
    tx_st30_mtl_session(mtl_handle dev_handle, struct st30_tx_ops *opts, memif_ops_t *memif_ops);
    ~tx_st30_mtl_session();

    int next_frame_cb(uint16_t *next_frame_idx, struct st30_tx_frame_meta *meta);
    int frame_done_cb(uint16_t frame_idx, struct st30_tx_frame_meta *meta);
};

/* Initialize MTL library */
mtl_handle inst_init(struct mtl_init_params *st_param);

/* Deinitialize MTL */
void mtl_deinit(mtl_handle dev_handle);

int frame_available_callback_wrapper(void *priv_data);

#endif /* __SESSION_MTL_H */
