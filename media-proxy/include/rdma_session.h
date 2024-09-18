/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __RDMA_SESSION_H
#define __RDMA_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mcm_dp.h>

#include "app_platform.h"
#include "libfabric_dev.h"
#include "shm_memif.h"
#include "utils.h"
#include "libfabric_ep.h"

typedef struct {
    size_t transfer_size;
    rdma_addr remote_addr;
    rdma_addr local_addr;
    enum direction dir;
} rdma_s_ops_t;

typedef struct {
    libfabric_ctx st;
    int idx;
    libfabric_ctx *rdma_ctx;
    ep_ctx_t *ep_ctx;

    int frame_done_cnt;
    int packet_done_cnt;

    bool stop;

    int fb_send;
    pthread_cond_t wake_cond;
    pthread_mutex_t wake_mutex;

    size_t transfer_size;
    size_t pkt_len;

    uint32_t fb_count; /* Frame buffer count. */

    /* memif parameters */
    memif_ops_t memif_ops;
    /* share memory arguments */
    memif_conn_args_t memif_conn_args;
    /* memif conenction handle */
    memif_conn_handle_t memif_conn;

    memif_buffer_t *shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    memif_socket_args_t memif_socket_args;
    memif_socket_handle_t memif_socket;
    pthread_t memif_event_thread;
} tx_rdma_session_context_t;

typedef struct {
    libfabric_ctx st;
    int idx;
    libfabric_ctx *rdma_ctx;
    ep_ctx_t *ep_ctx;

    bool stop;
    pthread_t frame_thread;

    int fb_recv;

    pthread_t app_thread;

    size_t transfer_size;
    int pkt_len;

    uint32_t fb_count; /* Frame buffer count. */

    /* share memory arguments */
    memif_socket_args_t memif_socket_args;
    memif_conn_args_t memif_conn_args;

    /* memif conenction handle */
    memif_socket_handle_t memif_socket;
    memif_conn_handle_t memif_conn;

    memif_buffer_t *shm_bufs;
    uint16_t shm_buf_num;
    uint8_t shm_ready;

    char name[32];
    pthread_t memif_event_thread;

    /* stat */
    int stat_frame_total_received;
    uint64_t stat_frame_first_rx_time;
    double expect_fps;
} rx_rdma_session_context_t;

/* TX: Create RDMA session */
tx_rdma_session_context_t *rdma_tx_session_create(libfabric_ctx *dev_handle, rdma_s_ops_t *opts,
                                                  memif_ops_t *memif_ops);

/* RX: Create RDMA session */
rx_rdma_session_context_t *rdma_rx_session_create(libfabric_ctx *dev_handle, rdma_s_ops_t *opts,
                                                  memif_ops_t *memif_ops);

/* TX: Stop rdma session */
void rdma_tx_session_stop(tx_rdma_session_context_t *pctx);

/* RX: Stop rdma session */
void rdma_rx_session_stop(rx_rdma_session_context_t *pctx);

/* TX: Destroy rdma session */
void rdma_tx_session_destroy(tx_rdma_session_context_t **ppctx);

/* RX: Destroy rdma session */
void rdma_rx_session_destroy(rx_rdma_session_context_t **ppctx);

#ifdef __cplusplus
}
#endif

#endif /* __RDMA_SESSION_H */
