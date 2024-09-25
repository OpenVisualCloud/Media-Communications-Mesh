/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <pthread.h>

#include "mtl.h"
#include "libfabric_dev.h"
#include "rdma_session.h"
#include "shm_memif.h"

/* rx_rdma_on_connect informs user about connected status.
 * private_ctx is used by user to identify connection.
 */
int rx_rdma_on_connect(memif_conn_handle_t conn, void *priv_data)
{
    rx_rdma_session_context_t *rx_ctx = (rx_rdma_session_context_t *)priv_data;
    int err;

    INFO("RX RDMA memif connected!");

    /* rx buffers */
    rx_ctx->shm_buf_num = 1;
    rx_ctx->shm_bufs = (memif_buffer_t *)malloc(sizeof(memif_buffer_t) * rx_ctx->shm_buf_num);
    if (!rx_ctx->shm_bufs) {
        ERROR("Failed to allocate memory");
        return -ENOMEM;
    }

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    rx_ctx->shm_ready = 1;

    return 0;
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int rx_rdma_on_disconnect(memif_conn_handle_t conn, void *priv_data)
{
    rx_rdma_session_context_t *rx_ctx = priv_data;
    memif_socket_handle_t socket;
    int err;

    if (conn == NULL || priv_data == NULL) {
        INFO("Invalid Parameters.");
        return -EINVAL;
    }

    // release session
    if (!rx_ctx->shm_ready) {
        return 0;
    }
    rx_ctx->shm_ready = 0;

    /* stop event polling thread */
    INFO("RX RDMA Stop poll event");
    socket = memif_get_socket_handle(conn);
    if (socket == NULL) {
        INFO("Invalid socket handle.");
        return -EINVAL;
    }

    err = memif_cancel_poll_event(socket);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("We are doomed...");
    }

    return 0;
}

int tx_rdma_on_connect(memif_conn_handle_t conn, void *priv_data)
{
    tx_rdma_session_context_t *tx_ctx = (tx_rdma_session_context_t *)priv_data;
    int err = 0;

    INFO("TX RDMA memif connected!");

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    tx_ctx->shm_ready = 1;

    print_memif_details(conn);

    return 0;
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int tx_rdma_on_disconnect(memif_conn_handle_t conn, void *priv_data)
{
    tx_rdma_session_context_t *tx_ctx = priv_data;
    memif_socket_handle_t socket;
    static int counter = 0;
    int err = 0;

    if (conn == NULL || priv_data == NULL) {
        INFO("Invalid Parameters.");
        return -EINVAL;
    }

    // release session
    if (!tx_ctx->shm_ready) {
        return 0;
    }
    tx_ctx->shm_ready = 0;

    /* stop event polling thread */
    INFO("TX Stop poll event");
    socket = memif_get_socket_handle(conn);
    if (socket == NULL) {
        INFO("Invalide socket handle.");
        return -EINVAL;
    }

    err = memif_cancel_poll_event(socket);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("We are doomed...");
    }

    return 0;
}

int tx_rdma_on_receive(memif_conn_handle_t conn, void *priv_data, uint16_t qid)
{
    tx_rdma_session_context_t *tx_ctx = (tx_rdma_session_context_t *)priv_data;
    memif_buffer_t shm_bufs = { 0 };
    uint16_t buf_num = 0;
    int err = 0;

    if (tx_ctx->stop) {
        INFO("TX session already stopped.");
        return -EINVAL;
    }

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    /* TODO: Use memif buffer directly. It has to be registered by libfabric */
    memcpy(tx_ctx->ep_ctx->data_buf, shm_bufs.data, shm_bufs.len);
    ep_send_buf(tx_ctx->ep_ctx, tx_ctx->ep_ctx->data_buf, shm_bufs.len);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    tx_ctx->fb_send++;

    return 0;
}
