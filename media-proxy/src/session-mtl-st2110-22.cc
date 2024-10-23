/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "session-mtl.h"

#if defined(MTL_ZERO_COPY)
static int query_ext_frame_callback_wrapper(void *priv, struct st_ext_frame *ext_frame,
                                            struct st22_rx_frame_meta *meta)
{
    rx_st22_mtl_session *s = (rx_st22_mtl_session *)priv;
    return s->query_ext_frame_cb(ext_frame, meta);
}

static int tx_frame_done_callback_wrapper(void *priv, struct st_frame *frame)
{
    tx_st22_mtl_session *s = (tx_st22_mtl_session *)priv;
    return s->frame_done_cb(frame);
}
#endif

void rx_st22_mtl_session::frame_thread()
{
    INFO("%s, start\n", __func__);
    while (!stop) {
        struct st_frame *frame = st22p_rx_get_frame(handle);
        if (!frame) { /* no frame */
            pthread_mutex_lock(&wake_mutex);
            if (!stop)
                pthread_cond_wait(&wake_cond, &wake_mutex);
            pthread_mutex_unlock(&wake_mutex);
            continue;
        }
        /* Verify frame data status. */
        if (frame->status == ST_FRAME_STATUS_CORRUPTED) {
            ERROR("[DBG] Received corrupted frame.\n");
        } else {
            consume_frame(frame);
        }
        st22p_rx_put_frame(handle, frame);
    }
}

/* RX: Create ST22P session */
rx_st22_mtl_session::rx_st22_mtl_session(mtl_handle dev_handle, struct st22p_rx_ops *opts,
                                         memif_ops_t *memif_ops)
    : handle(0), frame_thread_handle(0), shm_bufs(0), fb_recv(0)
{
    if (!dev_handle || !opts || !memif_ops) {
        cleanup();
        throw invalid_argument("Input parameters cannot be null");
    }

    st = dev_handle;

    struct st22p_rx_ops ops_rx = {0};
    mtl_memcpy(&ops_rx, opts, sizeof(struct st22p_rx_ops));

    ops_rx.priv = this; // app handle register to lib
    ops_rx.notify_frame_available = frame_available_callback_wrapper;

#if defined(MTL_ZERO_COPY)
    output_fmt = ops_rx.output_fmt;
    width = ops_rx.width;
    height = ops_rx.height;
    ops_rx.flags |= ST22P_RX_FLAG_EXT_FRAME;
    ops_rx.flags |= ST22P_RX_FLAG_RECEIVE_INCOMPLETE_FRAME;
    ops_rx.query_ext_frame = query_ext_frame_callback_wrapper;
#endif

    handle = st22p_rx_create(dev_handle, &ops_rx);
    if (!handle) {
        cleanup();
        throw runtime_error("Failed to create MTL RX ST22 session");
    }

    frame_size = st22p_rx_frame_size(handle);

    /* initialize share memory */
    shm_bufs = (memif_buffer_t *)malloc(sizeof(memif_buffer_t));
    if (!shm_bufs) {
        cleanup();
        throw runtime_error("Failed to allocate memory");
    }

    int ret = shm_init(memif_ops, frame_size, 2);
    if (ret < 0) {
        cleanup();
        throw runtime_error("Failed to initialize shared memory");
    }

    /* Start MTL session thread. */
    frame_thread_handle = new std::thread(&rx_st22_mtl_session::frame_thread, this);
    if (!frame_thread_handle) {
        cleanup();
        throw runtime_error("Failed to create thread");
    }
}

void rx_st22_mtl_session::cleanup()
{
    if (frame_thread_handle) {
        frame_thread_handle->join();
        delete frame_thread_handle;
        frame_thread_handle = 0;
    }

    if (handle) {
        st22p_rx_free(handle);
        handle = 0;
    }
    if (shm_bufs) {
        free(shm_bufs);
        shm_bufs = 0;
    }
}

rx_st22_mtl_session::~rx_st22_mtl_session()
{
    INFO("%s, fb_recv %d\n", __func__, fb_recv);
    cleanup();
}

void rx_st22_mtl_session::consume_frame(struct st_frame *frame)
{
    int err = 0;
    uint16_t qid = 0;
    memif_buffer_t *rx_bufs = NULL;
    uint16_t rx_buf_num = 0, rx = 0;

    if (!atomic_load_explicit(&shm_ready, memory_order_relaxed)) {
        INFO("%s memif not ready\n", __func__);
        return;
    }

#if defined(MTL_ZERO_COPY)
    rx_bufs = (memif_buffer_t *)frame->opaque;
    rx_buf_num = 1;
#else
    /* allocate memory */
    rx_bufs = shm_bufs;

    err = memif_buffer_alloc_timeout(memif_conn, qid, rx_bufs, 1, &rx_buf_num, frame_size, 10);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st22p consume_frame Failed to alloc memif buffer: %s", memif_strerror(err));
        return;
    }

    mtl_memcpy(rx_bufs->data, frame->addr[0], frame_size);
#endif

    /* Send to microservice application. */
    err = memif_tx_burst(memif_conn, qid, rx_bufs, rx_buf_num, &rx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st22p consume_frame memif_tx_burst: %s", memif_strerror(err));
    }

    fb_recv++;
}

/* TX: Create ST22P session */
tx_st22_mtl_session::tx_st22_mtl_session(mtl_handle dev_handle, struct st22p_tx_ops *opts,
                                         memif_ops_t *memif_ops)
    : handle(0), fb_send(0)
{
    if (!dev_handle || !opts || !memif_ops) {
        cleanup();
        throw invalid_argument("Input parameters cannot be null");
    }

    st = dev_handle;

    struct st22p_tx_ops ops_tx = {0};
    mtl_memcpy(&ops_tx, opts, sizeof(struct st22p_tx_ops));

    ops_tx.priv = this; // app handle register to lib
    ops_tx.notify_frame_available = frame_available_callback_wrapper;

#if defined(MTL_ZERO_COPY)
    ops_tx.notify_frame_done = tx_frame_done_callback_wrapper;
    ops_tx.flags |= ST22P_TX_FLAG_EXT_FRAME;
#endif

    handle = st22p_tx_create(dev_handle, &ops_tx);
    if (!handle) {
        cleanup();
        throw runtime_error("Failed to create MTL TX ST22 session");
    }

    frame_size = st22p_tx_frame_size(handle);

    /* initialize share memory */
    int ret = shm_init(memif_ops, frame_size, 2);
    if (ret < 0) {
        cleanup();
        throw runtime_error("Failed to initialize share memory");
    }
}

void tx_st22_mtl_session::cleanup()
{
    if (handle) {
        st22p_tx_free(handle);
        handle = 0;
    }
}

tx_st22_mtl_session::~tx_st22_mtl_session()
{
    INFO("%s, fb_send %d\n", __func__, fb_send);
    cleanup();
}

#if defined(MTL_ZERO_COPY)
int rx_st22_mtl_session::query_ext_frame_cb(struct st_ext_frame *ext_frame,
                                            struct st22_rx_frame_meta *meta)
{
    /* allocate memory */
    uint16_t qid = 0;
    uint16_t rx_buf_num = 0;

    if (!atomic_load_explicit(&shm_ready, memory_order_relaxed)) {
        ERROR("rx_st22p_query_ext_frame: MemIF connection not ready.");
        return -1;
    }

    int err = memif_buffer_alloc_timeout(memif_conn, qid, shm_bufs, 1, &rx_buf_num, frame_size, 10);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st22p_query_ext_frame: Failed to alloc memif buffer: %s", memif_strerror(err));
        return -1;
    }

    uint8_t planes = st_frame_fmt_planes(output_fmt);
    mtl_iova_t ext_fb_iova = source_begin_iova + ((uint8_t *)shm_bufs->data - source_begin);

    for (uint8_t plane = 0; plane < planes; plane++) { /* assume planes continuous */
        ext_frame->linesize[plane] = st_frame_least_linesize(output_fmt, width, plane);
        if (plane == 0) {
            ext_frame->addr[plane] = shm_bufs->data;
            ext_frame->iova[plane] = ext_fb_iova;
        } else {
            ext_frame->addr[plane] =
                (uint8_t *)ext_frame->addr[plane - 1] + ext_frame->linesize[plane - 1] * height;
            ext_frame->iova[plane] =
                ext_frame->iova[plane - 1] + ext_frame->linesize[plane - 1] * height;
        }
    }
    ext_frame->size = frame_size;

    /* save your private data here get it from st_frame.opaque */
    ext_frame->opaque = shm_bufs;

    return 0;
}

int rx_st22_mtl_session::on_connect_cb(memif_conn_handle_t conn)
{
    memif_region_details_t region;

    int err = memif_get_buffs_region(conn, &region);
    if (err) {
        ERROR("%s, Getting memory buffers from memif failed. \n", __func__);
        return err;
    }

    source_begin = (uint8_t *)region.addr;
    source_begin_iova_map_sz = region.size;
    source_begin_iova = mtl_dma_map(st, source_begin, region.size);
    if (source_begin_iova == MTL_BAD_IOVA) {
        ERROR("Fail to map DMA memory address.");
        return -1;
    }

    return session::on_connect_cb(conn);
}

int rx_st22_mtl_session::on_disconnect_cb(memif_conn_handle_t conn)
{
    if (atomic_load_explicit(&shm_ready, memory_order_relaxed) &&
        mtl_dma_unmap(st, source_begin, source_begin_iova, source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }

    return session::on_disconnect_cb(conn);
}

int tx_st22_mtl_session::frame_done_cb(struct st_frame *frame)
{
    uint16_t qid = 0;
    uint16_t buf_num = 1;

    if (!frame) {
        ERROR("%s, frame ptr is NULL\n", __func__);
        return -1;
    }

    memif_conn_handle_t conn = (memif_conn_handle_t)frame->opaque;
    if (!conn) {
        return -1;
    }

    /* return back frame buffer to memif. */
    int err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    return err;
}

int tx_st22_mtl_session::on_connect_cb(memif_conn_handle_t conn)
{
    memif_region_details_t region;

    int err = memif_get_buffs_region(conn, &region);
    if (err) {
        ERROR("%s, Getting memory buffers from memif failed. \n", __func__);
        return err;
    }

    source_begin = (uint8_t *)region.addr;
    source_begin_iova_map_sz = region.size;
    source_begin_iova = mtl_dma_map(st, source_begin, region.size);
    if (source_begin_iova == MTL_BAD_IOVA) {
        ERROR("Fail to map DMA memory address.");
        return -1;
    }

    return session::on_connect_cb(conn);
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int tx_st22_mtl_session::on_disconnect_cb(memif_conn_handle_t conn)
{
    if (atomic_load_explicit(&shm_ready, memory_order_relaxed) &&
        mtl_dma_unmap(st, source_begin, source_begin_iova, source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }

    return session::on_disconnect_cb(conn);
}
#endif

int tx_st22_mtl_session::on_receive_cb(memif_conn_handle_t conn, uint16_t qid)
{
    memif_buffer_t shm_bufs = {0};
    uint16_t buf_num = 0;

    if (stop) {
        INFO("TX session already stopped.");
        return -1;
    }

    /* receive packets from the shared memory */
    int err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }
    // INFO("memif_rx_burst: size: %d, num: %d", shm_bufs.len, buf_num);

    struct st_frame *frame = NULL;

    do {
        frame = st22p_tx_get_frame(handle);
        if (!frame) {
            pthread_mutex_lock(&wake_mutex);
            if (!stop)
                pthread_cond_wait(&wake_cond, &wake_mutex);
            pthread_mutex_unlock(&wake_mutex);
        }
    } while (!frame);

    /* Send out frame. */
#if defined(MTL_ZERO_COPY)
    struct st_ext_frame ext_frame = {0};
    ext_frame.addr[0] = shm_bufs.data;
    ext_frame.iova[0] = source_begin_iova + ((uint8_t *)shm_bufs.data - source_begin);
    ext_frame.linesize[0] = st_frame_least_linesize(frame->fmt, frame->width, 0);
    uint8_t planes = st_frame_fmt_planes(frame->fmt);
    for (uint8_t plane = 1; plane < planes; plane++) { /* assume planes continous */
        ext_frame.linesize[plane] = st_frame_least_linesize(frame->fmt, frame->width, plane);
        ext_frame.addr[plane] =
            (uint8_t *)ext_frame.addr[plane - 1] + ext_frame.linesize[plane - 1] * frame->height;
        ext_frame.iova[plane] =
            ext_frame.iova[plane - 1] + ext_frame.linesize[plane - 1] * frame->height;
    }
    ext_frame.size = shm_bufs.len;
    ext_frame.opaque = conn;

    st22p_tx_put_ext_frame(handle, frame, &ext_frame);
#else
    /* fill frame data. */
    mtl_memcpy(frame->addr[0], shm_bufs.data, shm_bufs.len);
    /* Send out frame. */
    st22p_tx_put_frame(handle, frame);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));
#endif

    fb_send++;

    return 0;
}
