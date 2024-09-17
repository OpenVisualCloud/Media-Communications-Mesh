/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>

#include "mtl.h"
#include "shm_memif.h"

int rx_st30_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    rx_st30_session_context_t* rx_ctx = (rx_st30_session_context_t*)priv_data;
    int err = 0;

    INFO("RX memif connected!");

    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    rx_ctx->fb_count = md.rx_queues[0].ring_size;

    free(buf);

    /* RX buffers */
    rx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * rx_ctx->fb_count);
    if (!rx_ctx->shm_bufs) {
        ERROR("Failed to allocate memory");
        return -ENOMEM;
    }
    rx_ctx->shm_buf_num = rx_ctx->fb_count;

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    rx_ctx->shm_ready = 1;

    return 0;
}

static void tx_st30_build_frame(memif_buffer_t shm_bufs, void* frame, size_t frame_size)
{
    mtl_memcpy(frame, shm_bufs.data, frame_size);
}

int tx_st30_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    tx_st30_session_context_t* tx_ctx = (tx_st30_session_context_t*)priv_data;
    memif_buffer_t shm_bufs = { 0 };
    uint16_t buf_num = 0;

    uint16_t producer_idx;
    struct st_tx_frame* framebuff;

    if (tx_ctx->stop) {
        INFO("TX session already stopped.");
        return -1;
    }

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    while (1) {
        st_pthread_mutex_lock(&tx_ctx->st30_wake_mutex);
        producer_idx = tx_ctx->framebuff_producer_idx;
        framebuff = &tx_ctx->framebuffs[producer_idx];
        if (ST_TX_FRAME_FREE != framebuff->stat) {
            /* not in free */
            if (!tx_ctx->stop)
                st_pthread_cond_wait(&tx_ctx->st30_wake_cond, &tx_ctx->st30_wake_mutex);
            st_pthread_mutex_unlock(&tx_ctx->st30_wake_mutex);
            continue;
        }
        st_pthread_mutex_unlock(&tx_ctx->st30_wake_mutex);
        break;
    }

    void* frame_addr = st30_tx_get_framebuffer(tx_ctx->handle, producer_idx);

    /* fill frame data. */
    tx_st30_build_frame(shm_bufs, frame_addr, tx_ctx->st30_frame_size);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    st_pthread_mutex_lock(&tx_ctx->st30_wake_mutex);
    framebuff->size = tx_ctx->st30_frame_size;
    framebuff->stat = ST_TX_FRAME_READY;
    /* point to next */
    producer_idx++;
    if (producer_idx >= tx_ctx->framebuff_cnt)
        producer_idx = 0;
    tx_ctx->framebuff_producer_idx = producer_idx;
    st_pthread_mutex_unlock(&tx_ctx->st30_wake_mutex);

    return 0;
}

int tx_st30_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    tx_st30_session_context_t* tx_ctx = (tx_st30_session_context_t*)priv_data;
    int err = 0;

    INFO("TX memif connected!");

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    tx_ctx->fb_count = md.tx_queues[0].ring_size;

    free(buf);

    /* TX buffers */
    tx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * tx_ctx->fb_count);
    tx_ctx->shm_buf_num = tx_ctx->fb_count;

    tx_ctx->shm_ready = 1;

    print_memif_details(conn);

    return 0;
}
