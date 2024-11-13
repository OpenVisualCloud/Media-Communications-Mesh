/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SESSION_MTL_H
#define __SESSION_MTL_H

#include <arpa/inet.h>
#include <mtl/st_pipeline_api.h> /* st2110 pipeline */
#include <mtl/st30_pipeline_api.h>

#include "session-base.h"

#include <queue>
#include <mutex>

#define MTL_ZERO_COPY

// Based on mtl/app/src/fmt.h
#ifndef ST_APP_PAYLOAD_TYPE_VIDEO
#define ST_APP_PAYLOAD_TYPE_ST30 (111)
#define ST_APP_PAYLOAD_TYPE_VIDEO (112)
#define ST_APP_PAYLOAD_TYPE_ST22 (114)
#endif

class MtlSession : public Session
{
  protected:
    mtl_handle st;

    pthread_cond_t wake_cond;
    pthread_mutex_t wake_mutex;

    volatile bool stop;

    MtlSession(memif_ops_t &memif_ops, mcm_payload_type payload, direction dir_type, mtl_handle st);
    virtual ~MtlSession();

  public:
    int frame_available_cb();
};

class RxSt20MtlSession : public MtlSession
{
    st20p_rx_ops ops;
    st20p_rx_handle handle;

    int fb_recv;
    size_t frame_size;

    std::thread *frame_thread_handle;

#if defined(MTL_ZERO_COPY)
    std::queue<memif_buffer_t> fifo;
    std::mutex fifo_mtx;
    uint8_t *source_begin;
    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;
#endif

    void copy_connection_params(const mcm_conn_param &request, std::string &dev_port);
    void consume_frame(struct st_frame *frame);
    void frame_thread();

#if defined(MTL_ZERO_COPY)
    int on_connect_cb(memif_conn_handle_t conn);
    int on_disconnect_cb(memif_conn_handle_t conn);
#endif

  public:
    int init();
    RxSt20MtlSession(mtl_handle dev_handle, const mcm_conn_param &request, std::string dev_port,
                     memif_ops_t &memif_ops);
    ~RxSt20MtlSession();

#if defined(MTL_ZERO_COPY)
    int query_ext_frame_cb(struct st_ext_frame *ext_frame, struct st20_rx_frame_meta *meta);
#endif
};

class TxSt20MtlSession : public MtlSession
{
    st20p_tx_ops ops;
    st20p_tx_handle handle;

    int fb_send;
    size_t frame_size;

#if defined(MTL_ZERO_COPY)
    uint8_t *source_begin;
    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;
#endif

    void copy_connection_params(const mcm_conn_param &request, std::string &dev_port);
    int on_receive_cb(memif_conn_handle_t conn, uint16_t qid);

#if defined(MTL_ZERO_COPY)
    int on_connect_cb(memif_conn_handle_t conn);
    int on_disconnect_cb(memif_conn_handle_t conn);
#endif

  public:
    int frame_done_cb(struct st_frame *frame);
    int init();
    TxSt20MtlSession(mtl_handle dev_handle, const mcm_conn_param &request, std::string dev_port,
                     memif_ops_t &memif_ops);
    ~TxSt20MtlSession();
};

class RxSt22MtlSession : public MtlSession
{
    st22p_rx_ops ops;
    st22p_rx_handle handle;

    int fb_recv;
    size_t frame_size;

    std::thread *frame_thread_handle;

#if defined(MTL_ZERO_COPY)
    std::queue<memif_buffer_t> fifo;
    std::mutex fifo_mtx;
    uint8_t *source_begin;
    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;
#endif

    void copy_connection_params(const mcm_conn_param &request, std::string &dev_port);
    void consume_frame(struct st_frame *frame);
    void frame_thread();

#if defined(MTL_ZERO_COPY)
    int on_connect_cb(memif_conn_handle_t conn);
    int on_disconnect_cb(memif_conn_handle_t conn);
#endif

  public:
    int init();
    RxSt22MtlSession(mtl_handle dev_handle, const mcm_conn_param &request, std::string dev_port,
                     memif_ops_t &memif_ops);
    ~RxSt22MtlSession();

#if defined(MTL_ZERO_COPY)
    int query_ext_frame_cb(struct st_ext_frame *ext_frame, struct st22_rx_frame_meta *meta);
#endif
};

class TxSt22MtlSession : public MtlSession
{
    st22p_tx_ops ops;
    st22p_tx_handle handle;

    int fb_send;
    size_t frame_size;

#if defined(MTL_ZERO_COPY)
    uint8_t *source_begin;
    mtl_iova_t source_begin_iova;
    size_t source_begin_iova_map_sz;
#endif

    void copy_connection_params(const mcm_conn_param &request, std::string &dev_port);
    int on_receive_cb(memif_conn_handle_t conn, uint16_t qid);

#if defined(MTL_ZERO_COPY)
    int on_connect_cb(memif_conn_handle_t conn);
    int on_disconnect_cb(memif_conn_handle_t conn);
#endif

  public:
    int frame_done_cb(struct st_frame *frame);
    int init();
    TxSt22MtlSession(mtl_handle dev_handle, const mcm_conn_param &request, std::string dev_port,
                     memif_ops_t &memif_ops);
    ~TxSt22MtlSession();
};

class RxSt30MtlSession : public MtlSession
{
    st30p_rx_ops ops;
    st30p_rx_handle handle;

    int fb_recv;

    std::thread *frame_thread_handle;

    void consume_frame(struct st30_frame *frame);
    void copy_connection_params(const mcm_conn_param &request, std::string &dev_port);
    void frame_thread();

  public:
    int init();
    RxSt30MtlSession(mtl_handle dev_handle, const mcm_conn_param &request, std::string dev_port,
                     memif_ops_t &memif_ops);
    ~RxSt30MtlSession();
};

class TxSt30MtlSession : public MtlSession
{
    st30p_tx_ops ops;
    st30p_tx_handle handle;

    int fb_send;

    void copy_connection_params(const mcm_conn_param &request, std::string &dev_port);
    int on_receive_cb(memif_conn_handle_t conn, uint16_t qid);

  public:
    int init();
    TxSt30MtlSession(mtl_handle dev_handle, const mcm_conn_param &request, std::string dev_port,
                     memif_ops_t &memif_ops);
    ~TxSt30MtlSession();
};

/* Initialize MTL library */
mtl_handle inst_init(struct mtl_init_params *st_param);

/* Deinitialize MTL */
void mtl_deinit(mtl_handle dev_handle);

st_frame_fmt get_st_frame_fmt(video_pixel_format mcm_frame_fmt);

int frame_available_callback_wrapper(void *priv_data);

#endif /* __SESSION_MTL_H */
