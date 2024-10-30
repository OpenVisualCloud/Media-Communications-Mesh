/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SESSION_RDMA_H
#define __SESSION_RDMA_H

#include <stdlib.h>
#include <mcm_dp.h>

#include "libfabric_dev.h"
#include "libfabric_ep.h"
#include "session-base.h"

typedef struct {
    memif_buffer_t shm_buf;
    bool used;
} shm_buf_info_t;

typedef struct {
    size_t transfer_size;
    rdma_addr remote_addr;
    rdma_addr local_addr;
    enum direction dir;
} rdma_s_ops_t;

class TxRdmaSession : public Session
{
    ep_cfg_t ep_cfg;
    ep_ctx_t *ep_ctx;

    volatile bool stop;
    std::thread *frame_thread_handle;

    int fb_send;
    size_t transfer_size;

    int on_connect_cb(memif_conn_handle_t conn);
    int on_receive_cb(memif_conn_handle_t conn, uint16_t qid);

    void frame_thread();
    void handle_sent_buffers();

  public:
    int init();
    TxRdmaSession(libfabric_ctx *dev_handle, const mcm_conn_param &request, memif_ops_t &memif_ops);
    ~TxRdmaSession();
};

class RxRdmaSession : public Session
{
    ep_cfg_t ep_cfg;
    ep_ctx_t *ep_ctx;

    volatile bool stop;
    std::thread *frame_thread_handle;

    int fb_recv;
    size_t transfer_size;

    shm_buf_info_t *shm_bufs;
    uint16_t shm_buf_num;

    int on_connect_cb(memif_conn_handle_t conn);

    void frame_thread();
    void handle_received_buffers();
    shm_buf_info_t *get_free_shm_buf();
    int pass_empty_buf_to_libfabric();

  public:
    int init();
    RxRdmaSession(libfabric_ctx *dev_handle, const mcm_conn_param &request, memif_ops_t &memif_ops);
    ~RxRdmaSession();
};

#endif /* __SESSION_RDMA_H */
