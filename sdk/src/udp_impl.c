/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "udp_impl.h"
#include "logger.h"

udp_context* mcm_create_connection_udp(mcm_conn_param* param)
{
    udp_context* udp_ctx = NULL;
    struct sockaddr_in tx_addr = {}, rx_addr = {};
    int sockfd = 0;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        log_error("Fail to create UDP socket.");
        return NULL;
    }

    if (param->type == is_rx) { /* RX */
        /* Bind address & port for RX. */
        /* Fill up RX address informations. */
        rx_addr.sin_family = AF_INET;
        rx_addr.sin_addr.s_addr = inet_addr(param->local_addr.ip);
        rx_addr.sin_port = htons(atoi(param->local_addr.port));

        /* Bind socket with RX address. */
        if (bind(sockfd, (const struct sockaddr*)&rx_addr, sizeof(rx_addr)) < 0) {
            log_error("Fail to bind socket for RX.");
            close(sockfd);
            return NULL;
        }

        /* Fill up TX address informations. */
        tx_addr.sin_family = AF_INET;
        tx_addr.sin_addr.s_addr = inet_addr(param->remote_addr.ip);
        tx_addr.sin_port = htons(atoi(param->remote_addr.port));
    } else { /* TX */
        /* Fill up RX address informations. */
        rx_addr.sin_family = AF_INET;
        rx_addr.sin_addr.s_addr = inet_addr(param->remote_addr.ip);
        rx_addr.sin_port = htons(atoi(param->remote_addr.port));
    }

    /* Fill information for UDP context. */
    udp_ctx = calloc(1, sizeof(udp_context));
    if (udp_ctx == NULL) {
        log_error("Outof Memory.");
        close(sockfd);
        return NULL;
    }

    udp_ctx->sockfd = sockfd;
    memcpy(&udp_ctx->tx_addr, &tx_addr, sizeof(struct sockaddr_in));
    memcpy(&udp_ctx->rx_addr, &rx_addr, sizeof(struct sockaddr_in));

    return udp_ctx;
}

void mcm_destroy_connection_udp(udp_context* pctx)
{
    if (pctx == NULL) {
        return;
    }

    close(pctx->sockfd);
    free(pctx);

    return;
}

mcm_buffer* mcm_alloc_buffer_udp(void* conn_ctx, size_t len)
{
    mcm_buffer* pbuf = NULL;

    pbuf = calloc(1, sizeof(mcm_buffer));
    if (pbuf == NULL) {
        log_error("Outof Memory.");
        return NULL;
    }

    pbuf->len = len;
    pbuf->data = calloc(1, len);
    if (pbuf->data == NULL) {
        log_error("Outof Memory.");
    }

    return pbuf;
}

void mcm_free_buffer_udp(void* conn_ctx, mcm_buffer** buf)
{
    mcm_buffer* pbuf = NULL;

    if (buf == NULL || *buf == NULL) {
        return;
    }

    pbuf = *buf;

    free(pbuf->data);
    free(pbuf);
    *buf = NULL;

    return;
}

int mcm_send_buffer_udp(void* conn_ctx, mcm_buffer* buf)
{
    int ret = 0;
    udp_context* ctx = NULL;
    void* data = NULL;
    ssize_t n = 0, len = 0;

    if (conn_ctx == NULL || buf == NULL) {
        log_error("Illegal Parameter.");
        return -1;
    }

    ctx = (udp_context*)conn_ctx;

    data = buf->data;
    len = buf->len;
    while (len > 0) {
        n = sendto(ctx->sockfd, data, len, MSG_CONFIRM,
            (const struct sockaddr*)&ctx->rx_addr, sizeof(ctx->rx_addr));
        if (n == -1) {
            log_error("Fail to send out data.");
            ret = -1;
            break;
        }

        len -= n;
        data += n;
    }

    return ret;
}

int mcm_recv_buffer_udp(void* conn_ctx, mcm_buffer* buf)
{
    int ret = 0;
    udp_context* ctx = NULL;
    socklen_t addrlen = 0;

    if (conn_ctx == NULL || buf == NULL) {
        log_error("Illegal Parameter.");
        return -1;
    }

    ctx = (udp_context*)conn_ctx;

    addrlen = sizeof(ctx->tx_addr);

    ret = recvfrom(ctx->sockfd, buf->data, buf->len, MSG_WAITALL,
        (struct sockaddr*)&ctx->tx_addr, &addrlen);
    if (ret == -1) {
        log_error("Fail to send out data.");
    }

    return ret;
}
