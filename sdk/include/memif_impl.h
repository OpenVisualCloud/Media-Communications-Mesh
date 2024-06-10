/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MCM_DP_SDK_MEMIF_H__
#define __MCM_DP_SDK_MEMIF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mcm_dp.h"

#define MEMIF_BUFFER_NUM 64
typedef struct {
    /* connection status */
    _Atomic uint8_t is_connected;

    /* memif socket */
    memif_socket_handle_t sockfd;
    /* memif conenction handle */
    memif_conn_handle_t conn;
    /* memif interface id */
    uint32_t memif_if_id;
    /* transmit queue id */
    uint16_t qid;

    size_t buffer_size;
    /* buffer queue */
    memif_buffer_t* shm_bufs;
    /* number of buffers pointing to shared memory */
    uint16_t buf_num;

    /* staging buffer */
    memif_buffer_t working_bufs[MEMIF_BUFFER_NUM];
    int working_idx;
} memif_conn_context;

typedef struct {
    uint8_t is_master;
    char app_name[32];
    char interface_name[32];
    uint32_t interface_id;
    char socket_path[108];
} memif_ops_t;

/* Create memif connection . */
mcm_conn_context* mcm_create_connection_memif(mcm_conn_param* svc_args, memif_conn_param* memif_args);

/* Destroy memif connection. */
void mcm_destroy_connection_memif(memif_conn_context* pctx);

/* Alloc buffer from queue. */
// void* memif_alloc_buffer(void* conn_ctx, size_t size);

/* Send out video frame on TX side. */
// int memif_send_buffer(void* conn_ctx, mcm_buffer* buf);

/* Receive video frame on RX side. */
// int memif_recv_buffer(void* conn_ctx, mcm_buffer* buf);

/* Return video frame buffer to queue. */
// void memif_free_buffer(void* conn_ctx, mcm_buffer* buf);

/* Alloc video frame buffer from buffer queue. */
mcm_buffer* memif_dequeue_buffer(mcm_conn_context* conn_ctx, int timeout, int* error_code);

/* Return video frame buffer to buffer queue. */
int memif_enqueue_buffer(mcm_conn_context* conn_ctx, mcm_buffer* buf);

#ifdef __cplusplus
}
#endif

#endif /* __MCM_DP_SDK_MEMIF_H__ */