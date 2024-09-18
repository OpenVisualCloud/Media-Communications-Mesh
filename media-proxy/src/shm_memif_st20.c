/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>

#include "mtl.h"
#include "shm_memif.h"

static void tx_st20p_build_frame(memif_buffer_t shm_bufs, struct st_frame* frame)
{
    mtl_memcpy(frame->addr[0], shm_bufs.data, shm_bufs.len);
}

/* informs user about connected status. private_ctx is used by user to identify
 * connection */
int rx_st20p_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    rx_session_context_t* rx_ctx = (rx_session_context_t*)priv_data;
    int err = 0;

    INFO("RX memif connected!");

#if defined(ZERO_COPY)
    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    rx_ctx->fb_count = md.tx_queues[0].ring_size;
    rx_ctx->source_begin = md.regions[1].addr;
    rx_ctx->source_begin_iova_map_sz = md.regions[1].size;
    rx_ctx->source_begin_iova = mtl_dma_map(rx_ctx->st, rx_ctx->source_begin, md.regions[1].size);
    if (rx_ctx->source_begin_iova == MTL_BAD_IOVA) {
        ERROR("Fail to map DMA memory address.");
        return -1;
    }

    free(buf);
#else
    rx_ctx->fb_count = 3;
#endif

    /* rx buffers */
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

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int rx_st20p_on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    int err = 0;
    rx_session_context_t* rx_ctx = priv_data;
    memif_socket_handle_t socket;

    if (conn == NULL) {
        return 0;
    }

    if (conn == NULL || priv_data == NULL) {
        INFO("Invalid Parameters.");
        return -1;
    }

    // release session
    if (rx_ctx->shm_ready == 0) {
        return 0;
    }
    rx_ctx->shm_ready = 0;

    // mtl_st20p_rx_session_destroy(&rx_ctx);

#if defined(ZERO_COPY)
    if (mtl_dma_unmap(rx_ctx->st, rx_ctx->source_begin, rx_ctx->source_begin_iova, rx_ctx->source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }
#endif

    /* stop event polling thread */
    INFO("RX Stop poll event\n");
    socket = memif_get_socket_handle(conn);
    if (socket == NULL) {
        INFO("Invalide socket handle.");
        return -1;
    }

    err = memif_cancel_poll_event(socket);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("We are doomed...");
    }

    // free(priv_data);

    return 0;
}

int tx_st20p_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    tx_session_context_t* tx_ctx = (tx_session_context_t*)priv_data;
    int err = 0;

    INFO("TX memif connected!");

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

#if defined(ZERO_COPY)
    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    tx_ctx->source_begin = md.regions[1].addr;
    tx_ctx->source_begin_iova_map_sz = md.regions[1].size;
    tx_ctx->source_begin_iova = mtl_dma_map(tx_ctx->st, tx_ctx->source_begin, md.regions[1].size);
    if (tx_ctx->source_begin_iova == MTL_BAD_IOVA) {
        ERROR("Fail to map DMA memory address.");
        return -1;
    }

    free(buf);
#endif

    tx_ctx->shm_ready = 1;

    print_memif_details(conn);

    return 0;
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int tx_st20p_on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    static int counter = 0;
    int err = 0;
    tx_session_context_t* tx_ctx = priv_data;
    memif_socket_handle_t socket;

    if (conn == NULL || priv_data == NULL) {
        INFO("Invalid Parameters.");
        return -1;
    }

    // release session
    if (tx_ctx->shm_ready == 0) {
        return 0;
    }
    tx_ctx->shm_ready = 0;

    // mtl_st20p_tx_session_destroy(&tx_ctx);

#if defined(ZERO_COPY)
    if (mtl_dma_unmap(tx_ctx->st, tx_ctx->source_begin, tx_ctx->source_begin_iova, tx_ctx->source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }
#endif

    /* stop event polling thread */
    INFO("TX Stop poll event");
    socket = memif_get_socket_handle(conn);
    if (socket == NULL) {
        INFO("Invalide socket handle.");
        return -1;
    }

    err = memif_cancel_poll_event(socket);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("We are doomed...");
    }

    // free(priv_data);

    return 0;
}

int tx_st20p_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    tx_session_context_t* tx_ctx = (tx_session_context_t*)priv_data;
    memif_buffer_t shm_bufs = { 0 };
    uint16_t buf_num = 0;

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
    // INFO("memif_rx_burst: size: %d, num: %d", shm_bufs.len, buf_num);

    st20p_tx_handle handle = tx_ctx->handle;
    struct st_frame* frame = NULL;

    do {
        frame = st20p_tx_get_frame(handle);
        if (!frame) {
            st_pthread_mutex_lock(&tx_ctx->wake_mutex);
            if (!tx_ctx->stop)
                st_pthread_cond_wait(&tx_ctx->wake_cond, &tx_ctx->wake_mutex);
            st_pthread_mutex_unlock(&tx_ctx->wake_mutex);
        }
    } while (!frame);

    /* Send out frame. */
#if defined(ZERO_COPY)
    struct st_ext_frame ext_frame = { 0 };
    ext_frame.addr[0] = shm_bufs.data;
    ext_frame.iova[0] = tx_ctx->source_begin_iova + ((uint8_t*)shm_bufs.data - tx_ctx->source_begin);
    ext_frame.linesize[0] = st_frame_least_linesize(frame->fmt, frame->width, 0);
    uint8_t planes = st_frame_fmt_planes(frame->fmt);
    for (uint8_t plane = 1; plane < planes; plane++) { /* assume planes continous */
        ext_frame.linesize[plane] = st_frame_least_linesize(frame->fmt, frame->width, plane);
        ext_frame.addr[plane] = (uint8_t*)ext_frame.addr[plane - 1] + ext_frame.linesize[plane - 1] * frame->height;
        ext_frame.iova[plane] = ext_frame.iova[plane - 1] + ext_frame.linesize[plane - 1] * frame->height;
    }
    ext_frame.size = shm_bufs.len;
    ext_frame.opaque = conn;

    st20p_tx_put_ext_frame(handle, frame, &ext_frame);
#else
    /* fill frame data. */
    tx_st20p_build_frame(shm_bufs, frame);
    /* Send out frame. */
    st20p_tx_put_frame(handle, frame);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));
#endif

    // INFO("%s send frame %d", __func__, tx_ctx->fb_send);
    tx_ctx->fb_send++;

    return 0;
}
