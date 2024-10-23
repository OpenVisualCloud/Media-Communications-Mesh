/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "session-mtl.h"

enum st_tx_frame_status {
    ST_TX_FRAME_FREE = 0,
    ST_TX_FRAME_READY,
    ST_TX_FRAME_IN_TRANSMITTING,
    ST_TX_FRAME_STATUS_MAX,
};

struct st_tx_frame {
    enum st_tx_frame_status stat;
};

static int frame_ready_callback_wrapper(void *priv, void *frame, struct st30_rx_frame_meta *meta)
{
    rx_st30_mtl_session *s = (rx_st30_mtl_session *)priv;
    return s->frame_ready_cb(frame, meta);
}

static int next_frame_callback_wrapper(void *priv, uint16_t *next_frame_idx,
                                       struct st30_tx_frame_meta *meta)
{
    tx_st30_mtl_session *s = (tx_st30_mtl_session *)priv;
    return s->next_frame_cb(next_frame_idx, meta);
}

static int frame_done_callback_wrapper(void *priv, uint16_t frame_idx,
                                       struct st30_tx_frame_meta *meta)
{
    tx_st30_mtl_session *s = (tx_st30_mtl_session *)priv;
    return s->frame_done_cb(frame_idx, meta);
}

/* RX: Create ST30 session */
rx_st30_mtl_session::rx_st30_mtl_session(mtl_handle dev_handle, struct st30_rx_ops *opts,
                                         memif_ops_t *memif_ops)
    : handle(0), fb_recv(0), shm_bufs(0)
{
    if (!dev_handle || !opts || !memif_ops) {
        cleanup();
        throw invalid_argument("Input parameters cannot be null");
    }

    st = dev_handle;

    struct st30_rx_ops ops_rx = {0};
    mtl_memcpy(&ops_rx, opts, sizeof(struct st30_rx_ops));

    ops_rx.priv = this; // app handle register to lib
    ops_rx.notify_frame_ready = frame_ready_callback_wrapper;
    ops_rx.notify_rtp_ready = frame_available_callback_wrapper;

    pkt_len = st30_get_packet_size(opts->fmt, opts->ptime, opts->sampling, opts->channel);

    ops_rx.framebuff_size = pkt_len;

    /* initialize share memory */
    shm_bufs = (memif_buffer_t *)malloc(sizeof(memif_buffer_t));
    if (!shm_bufs) {
        cleanup();
        throw runtime_error("Failed to allocate memory");
    }

    int ret = shm_init(memif_ops, pkt_len, 4);
    if (ret < 0) {
        cleanup();
        throw runtime_error("Failed to initialize shared memory");
    }

    handle = st30_rx_create(dev_handle, &ops_rx);
    if (!handle) {
        cleanup();
        throw runtime_error("Failed to initialize MTL RX ST30 session\n");
    }
}

void rx_st30_mtl_session::cleanup()
{
    if (handle) {
        st30_rx_free(handle);
        handle = 0;
    }

    if (shm_bufs) {
        free(shm_bufs);
        shm_bufs = 0;
    }
}

rx_st30_mtl_session::~rx_st30_mtl_session()
{
    INFO("%s, fb_recv %d\n", __func__, fb_recv);
    cleanup();
}

int rx_st30_mtl_session::frame_ready_cb(void *frame, struct st30_rx_frame_meta *meta)
{
    uint16_t qid = 0;
    uint16_t tx_buf_num = 0, tx = 0;

    if (!atomic_load_explicit(&shm_ready, memory_order_relaxed)) {
        INFO("%s memif not ready\n", __func__);
        return -1;
    }

    int err = memif_buffer_alloc_timeout(memif_conn, qid, shm_bufs, 1, &tx_buf_num, pkt_len, 10);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st30 onsume_frame Failed to alloc memif buffer: %s", memif_strerror(err));
        return -1;
    }

    /* Fill the shared-memory buffer. */
    mtl_memcpy((uint8_t *)shm_bufs->data, frame, pkt_len);

    st30_rx_put_framebuff(handle, frame);

    /* Send to microservice application. */
    err = memif_tx_burst(memif_conn, qid, shm_bufs, tx_buf_num, &tx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_tx_burst: %s", memif_strerror(err));
        return -1;
    }
    return 0;
}

int tx_st30_mtl_session::next_frame_cb(uint16_t *next_frame_idx, struct st30_tx_frame_meta *meta)
{
    int ret = 0;
    MTL_MAY_UNUSED(meta);

    pthread_mutex_lock(&wake_mutex);
    struct st_tx_frame *framebuff = &framebuffs[framebuff_consumer_idx];
    if (ST_TX_FRAME_READY == framebuff->stat) {
        framebuff->stat = ST_TX_FRAME_IN_TRANSMITTING;
        *next_frame_idx = framebuff_consumer_idx;
        /* point to next */
        framebuff_consumer_idx++;
        if (framebuff_consumer_idx >= framebuff_cnt)
            framebuff_consumer_idx = 0;
    } else {
        /* not ready */
        INFO("%s, idx %u err stat %d\n", __func__, framebuff_consumer_idx, framebuff->stat);
        ret = -EIO;
    }
    pthread_cond_signal(&wake_cond);
    pthread_mutex_unlock(&wake_mutex);
    return ret;
}

int tx_st30_mtl_session::frame_done_cb(uint16_t frame_idx, struct st30_tx_frame_meta *meta)
{
    int ret = 0;
    MTL_MAY_UNUSED(meta);

    pthread_mutex_lock(&wake_mutex);
    struct st_tx_frame *framebuff = &framebuffs[frame_idx];
    if (ST_TX_FRAME_IN_TRANSMITTING == framebuff->stat) {
        framebuff->stat = ST_TX_FRAME_FREE;
    } else {
        ret = -EIO;
        DEBUG("%s, err status %d for frame %u\n", __func__, framebuff->stat, frame_idx);
    }
    pthread_cond_signal(&wake_cond);
    pthread_mutex_unlock(&wake_mutex);
    return ret;
}

/* TX: Create ST30 session */
tx_st30_mtl_session::tx_st30_mtl_session(mtl_handle dev_handle, struct st30_tx_ops *opts,
                                         memif_ops_t *memif_ops)
    : handle(0), framebuffs(0), fb_send(0)
{
    if (!dev_handle || !opts || !memif_ops) {
        cleanup();
        throw invalid_argument("Input parameters cannot be null");
    }

    framebuff_cnt = 2;
    framebuffs = (struct st_tx_frame *)calloc(framebuff_cnt, sizeof(*framebuffs));
    if (!framebuffs) {
        cleanup();
        throw runtime_error("Failed to allocate memory");
    }

    for (uint16_t j = 0; j < framebuff_cnt; j++) {
        framebuffs[j].stat = ST_TX_FRAME_FREE;
    }

    st = dev_handle;

    struct st30_tx_ops ops_tx = {0};
    mtl_memcpy(&ops_tx, opts, sizeof(struct st30_tx_ops));

    ops_tx.priv = this; // app handle register to lib
    ops_tx.get_next_frame = next_frame_callback_wrapper;
    ops_tx.notify_frame_done = frame_done_callback_wrapper;

    sampling = ops_tx.sampling;
    pkt_len = st30_get_packet_size(ops_tx.fmt, ops_tx.ptime, ops_tx.sampling, ops_tx.channel);

    ops_tx.framebuff_size = pkt_len;
    ops_tx.framebuff_cnt = framebuff_cnt;

    handle = st30_tx_create(dev_handle, &ops_tx);
    if (!handle) {
        cleanup();
        throw runtime_error("Failed to create MTL TX ST30 session.");
    }

    /* initialize share memory */
    int ret = shm_init(memif_ops, pkt_len, 4);
    if (ret < 0) {
        cleanup();
        throw runtime_error("Failed to initialize shared memory");
    }
}

void tx_st30_mtl_session::cleanup()
{
    if (handle) {
        st30_tx_free(handle);
        handle = 0;
    }

    if (framebuffs) {
        free(framebuffs);
        framebuffs = 0;
    }
}

tx_st30_mtl_session::~tx_st30_mtl_session()
{
    INFO("%s, fb_send %d\n", __func__, fb_send);
    cleanup();
}

int tx_st30_mtl_session::on_receive_cb(memif_conn_handle_t conn, uint16_t qid)
{
    memif_buffer_t shm_bufs = {0};
    uint16_t buf_num = 0;

    struct st_tx_frame *framebuff;

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

    while (1) {
        pthread_mutex_lock(&wake_mutex);
        framebuff = &framebuffs[framebuff_producer_idx];
        if (ST_TX_FRAME_FREE != framebuff->stat) {
            /* not in free */
            if (!stop)
                pthread_cond_wait(&wake_cond, &wake_mutex);
            pthread_mutex_unlock(&wake_mutex);
            continue;
        }
        pthread_mutex_unlock(&wake_mutex);
        break;
    }

    void *frame_addr = st30_tx_get_framebuffer(handle, framebuff_producer_idx);

    /* fill frame data. */
    mtl_memcpy(frame_addr, shm_bufs.data, pkt_len);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    pthread_mutex_lock(&wake_mutex);
    framebuff->stat = ST_TX_FRAME_READY;
    /* point to next */
    framebuff_producer_idx++;
    if (framebuff_producer_idx >= framebuff_cnt)
        framebuff_producer_idx = 0;
    pthread_mutex_unlock(&wake_mutex);

    return 0;
}
