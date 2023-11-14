/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MCM_DP_UDP_H
#define __MCM_DP_UDP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mcm_dp.h"
#include <netinet/in.h>

typedef struct _udp_context {
    int sockfd;
    struct sockaddr_in rx_addr;
    struct sockaddr_in tx_addr;
} udp_context;

/* Create MCM DP connect session for application. */
udp_context* mcm_create_connection_udp(mcm_conn_param* param);

/* Destroy MCM DP connection. */
void mcm_destroy_connection_udp(udp_context* conn_ctx);

/* Alloc buffer from queue. */
mcm_buffer* mcm_alloc_buffer_udp(void* conn_ctx, size_t size);

/* Send out video frame on TX side. */
int mcm_send_buffer_udp(void* conn_ctx, mcm_buffer* buf);

/* Receive video frame on RX side. */
int mcm_recv_buffer_udp(void* conn_ctx, mcm_buffer* buf);

/* Return video frame buffer to queue. */
void mcm_free_buffer_udp(void* conn_ctx, mcm_buffer** buf);

#ifdef __cplusplus
}
#endif

#endif /* __MCM_DP_UDP_H */