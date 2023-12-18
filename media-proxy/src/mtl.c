/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "mtl.h"
#include "shm_memif.h"
#include <mcm_dp.h>

static void rx_st30_consume_frame(rx_st30_session_context_t* s, void* frame);
static int tx_st30_shm_deinit(tx_st30_session_context_t* pctx);
static int rx_st30_shm_deinit(rx_st30_session_context_t* pctx);
static int tx_st40_shm_deinit(tx_st40_session_context_t* pctx);
static int rx_st40_shm_deinit(rx_st40_session_context_t* pctx);

static int tx_st30_next_frame(void* priv, uint16_t* next_frame_idx,
    struct st30_tx_frame_meta* meta)
{
    tx_st30_session_context_t* s = priv;
    int ret;
    uint16_t consumer_idx = s->framebuff_consumer_idx;
    struct st_tx_frame* framebuff = &s->framebuffs[consumer_idx];
    MTL_MAY_UNUSED(meta);

    st_pthread_mutex_lock(&s->st30_wake_mutex);
    if (ST_TX_FRAME_READY == framebuff->stat) {
        DEBUG("%s(%d), next frame idx %u, epoch %" PRIu64 ", tai %" PRIu64 "\n", __func__,
            s->idx, consumer_idx, meta->epoch,
            st10_get_tai(meta->tfmt, meta->timestamp, st30_get_sample_rate(s->sampling)));
        ret = 0;
        framebuff->stat = ST_TX_FRAME_IN_TRANSMITTING;
        *next_frame_idx = consumer_idx;
        /* point to next */
        consumer_idx++;
        if (consumer_idx >= s->framebuff_cnt)
            consumer_idx = 0;
        s->framebuff_consumer_idx = consumer_idx;
    } else {
        /* not ready */
        DEBUG("%s(%d), idx %u err stat %d\n", __func__, s->idx, consumer_idx, framebuff->stat);
        ret = -EIO;
    }
    st_pthread_cond_signal(&s->st30_wake_cond);
    st_pthread_mutex_unlock(&s->st30_wake_mutex);
    return ret;
}

static int tx_st30_frame_done(void* priv, uint16_t frame_idx,
    struct st30_tx_frame_meta* meta)
{
    tx_st30_session_context_t* s = priv;
    int ret;
    struct st_tx_frame* framebuff = &s->framebuffs[frame_idx];
    MTL_MAY_UNUSED(meta);

    st_pthread_mutex_lock(&s->st30_wake_mutex);
    if (ST_TX_FRAME_IN_TRANSMITTING == framebuff->stat) {
        ret = 0;
        framebuff->stat = ST_TX_FRAME_FREE;
        DEBUG("%s(%d), done frame idx %u, epoch %" PRIu64 ", tai %" PRIu64 "\n", __func__,
            s->idx, frame_idx, meta->epoch,
            st10_get_tai(meta->tfmt, meta->timestamp, st30_get_sample_rate(s->sampling)));
    } else {
        ret = -EIO;
        DEBUG("%s(%d), err status %d for frame %u\n", __func__, s->idx, framebuff->stat,
            frame_idx);
    }
    st_pthread_cond_signal(&s->st30_wake_cond);
    st_pthread_mutex_unlock(&s->st30_wake_mutex);

    s->st30_frame_done_cnt++;
    DEBUG("%s(%d), framebuffer index %d\n", __func__, s->idx, frame_idx);
    return ret;
}

static int tx_st30_rtp_done(void* priv)
{
    tx_st30_session_context_t* s = priv;
    st_pthread_mutex_lock(&s->st30_wake_mutex);
    st_pthread_cond_signal(&s->st30_wake_cond);
    st_pthread_mutex_unlock(&s->st30_wake_mutex);
    s->st30_packet_done_cnt++;
    return 0;
}

static int tx_st40_next_frame(void* priv, uint16_t* next_frame_idx,
    struct st40_tx_frame_meta* meta)
{
    tx_st40_session_context_t* s = priv;
    int ret;
    uint16_t consumer_idx = s->framebuff_consumer_idx;
    struct st_tx_frame* framebuff = &s->framebuffs[consumer_idx];
    MTL_MAY_UNUSED(meta);

    st_pthread_mutex_lock(&s->st40_wake_mutex);
    if (ST_TX_FRAME_READY == framebuff->stat) {
        DEBUG("%s(%d), next frame idx %u, epoch %" PRIu64 ", tai %" PRIu64 "\n", __func__,
            s->idx, consumer_idx, meta->epoch,
            st10_get_tai(meta->tfmt, meta->timestamp, ST10_VIDEO_SAMPLING_RATE_90K));
        ret = 0;
        framebuff->stat = ST_TX_FRAME_IN_TRANSMITTING;
        *next_frame_idx = consumer_idx;
        /* point to next */
        consumer_idx++;
        if (consumer_idx >= s->framebuff_cnt)
            consumer_idx = 0;
        s->framebuff_consumer_idx = consumer_idx;
    } else {
        /* not ready */
        DEBUG("%s(%d), idx %u err stat %d\n", __func__, s->idx, consumer_idx, framebuff->stat);
        ret = -EIO;
    }
    st_pthread_cond_signal(&s->st40_wake_cond);
    st_pthread_mutex_unlock(&s->st40_wake_mutex);
    return ret;
}

static int tx_st40_frame_done(void* priv, uint16_t frame_idx,
    struct st40_tx_frame_meta* meta)
{
    tx_st40_session_context_t* s = priv;
    int ret;
    struct st_tx_frame* framebuff = &s->framebuffs[frame_idx];
    MTL_MAY_UNUSED(meta);

    st_pthread_mutex_lock(&s->st40_wake_mutex);
    if (ST_TX_FRAME_IN_TRANSMITTING == framebuff->stat) {
        ret = 0;
        framebuff->stat = ST_TX_FRAME_FREE;
        DEBUG("%s(%d), done frame idx %u, epoch %" PRIu64 ", tai %" PRIu64 "\n", __func__,
            s->idx, frame_idx, meta->epoch,
            st10_get_tai(meta->tfmt, meta->timestamp, ST10_VIDEO_SAMPLING_RATE_90K));
    } else {
        ret = -EIO;
        DEBUG("%s(%d), err status %d for frame %u\n", __func__, s->idx, framebuff->stat,
            frame_idx);
    }
    st_pthread_cond_signal(&s->st40_wake_cond);
    st_pthread_mutex_unlock(&s->st40_wake_mutex);

    s->st40_frame_done_cnt++;
    DEBUG("%s(%d), framebuffer index %d\n", __func__, s->idx, frame_idx);
    return ret;
}

static int tx_st40_rtp_done(void* priv)
{
    tx_st40_session_context_t* s = priv;
    st_pthread_mutex_lock(&s->st40_wake_mutex);
    st_pthread_cond_signal(&s->st40_wake_cond);
    st_pthread_mutex_unlock(&s->st40_wake_mutex);
    s->st40_packet_done_cnt++;
    return 0;
}

void st_rx_debug_dump(struct st20p_rx_ops ops)
{
    INFO("Parse RX Session Ops ...");
    INFO("name          : %s", ops.name);
    INFO("priv          : %p", ops.priv);
    INFO("sip_addr      : %u, %u, %u, %u",
        ops.port.sip_addr[MTL_PORT_P][0],
        ops.port.sip_addr[MTL_PORT_P][1],
        ops.port.sip_addr[MTL_PORT_P][2],
        ops.port.sip_addr[MTL_PORT_P][3]);
    INFO("num_port      : %u", ops.port.num_port);
    INFO("port          : %s", ops.port.port[MTL_PORT_P]);
    INFO("udp_port      : %u", ops.port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %u", ops.port.payload_type);
    INFO("flag          : %u", ops.flags);
    INFO("width         : %u", ops.width);
    INFO("height        : %u", ops.height);
    INFO("fps           : %d", ops.fps);
    INFO("transport_fmt : %d", ops.transport_fmt);
    INFO("output_fmt    : %d", ops.output_fmt);
    INFO("device        : %d", ops.device);
    INFO("framebuff_cnt : %d", ops.framebuff_cnt);
}

void st_rx_st22p_debug_dump(struct st22p_rx_ops ops)
{
    INFO("Parse RX Session Ops ...");
    INFO("name          : %s", ops.name);
    INFO("priv          : %p", ops.priv);
    INFO("sip_addr      : %u, %u, %u, %u",
        ops.port.sip_addr[MTL_PORT_P][0],
        ops.port.sip_addr[MTL_PORT_P][1],
        ops.port.sip_addr[MTL_PORT_P][2],
        ops.port.sip_addr[MTL_PORT_P][3]);
    INFO("num_port      : %u", ops.port.num_port);
    INFO("port          : %s", ops.port.port[MTL_PORT_P]);
    INFO("udp_port      : %u", ops.port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %u", ops.port.payload_type);
    INFO("flag          : %u", ops.flags);
    INFO("width         : %u", ops.width);
    INFO("height        : %u", ops.height);
    INFO("fps           : %d", ops.fps);
    INFO("output_fmt    : %d", ops.output_fmt);
    INFO("device        : %d", ops.device);
    INFO("framebuff_cnt : %d", ops.framebuff_cnt);
}

void st_tx_debug_dump(struct st20p_tx_ops ops)
{
    INFO("Parse TX Session Ops ...");
    INFO("name          : %s", ops.name);
    INFO("priv          : %p", ops.priv);
    printf("INFO: dip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %u", ops.port.dip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("port          : %s", ops.port.port[MTL_PORT_P]);
    INFO("num_port      : %u", ops.port.num_port);
    INFO("udp_port      : %u", ops.port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %u", ops.port.payload_type);
    INFO("flag          : %u", ops.flags);
    INFO("width         : %u", ops.width);
    INFO("height        : %u", ops.height);
    INFO("fps           : %d", ops.fps);
    INFO("input_fmt     : %d", ops.input_fmt);
    INFO("transport_fmt : %d", ops.transport_fmt);
    INFO("device        : %d", ops.device);
    INFO("framebuff_cnt : %d", ops.framebuff_cnt);
}

void st_tx_st22p_debug_dump(struct st22p_tx_ops ops)
{
    INFO("Parse TX Session Ops ...");
    INFO("name          : %s", ops.name);
    INFO("priv          : %p", ops.priv);
    printf("INFO: dip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %u", ops.port.dip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("port          : %s", ops.port.port[MTL_PORT_P]);
    INFO("num_port      : %u", ops.port.num_port);
    INFO("udp_port      : %u", ops.port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %u", ops.port.payload_type);
    INFO("flag          : %u", ops.flags);
    INFO("width         : %u", ops.width);
    INFO("height        : %u", ops.height);
    INFO("fps           : %d", ops.fps);
    INFO("input_fmt     : %d", ops.input_fmt);
    INFO("device        : %d", ops.device);
    INFO("framebuff_cnt : %d", ops.framebuff_cnt);
}

static int rx_st20p_frame_available(void* priv)
{
    rx_session_context_t* s = priv;

    st_pthread_mutex_lock(&s->wake_mutex);
    st_pthread_cond_signal(&s->wake_cond);
    st_pthread_mutex_unlock(&s->wake_mutex);

    return 0;
}

static int rx_st22p_frame_available(void* priv)
{
    rx_st22p_session_context_t* s = priv;

    st_pthread_mutex_lock(&s->st22p_wake_mutex);
    st_pthread_cond_signal(&s->st22p_wake_cond);
    st_pthread_mutex_unlock(&s->st22p_wake_mutex);

    return 0;
}

#if defined(ZERO_COPY)
static int rx_st20p_query_ext_frame(void* priv, struct st_ext_frame* ext_frame,
    struct st20_rx_frame_meta* meta)
{
    rx_session_context_t* rx_ctx = priv;
    int err = 0;

    /* allocate memory */
    uint16_t qid = 0;
    uint16_t buf_num = 1;
    uint32_t buf_size = rx_ctx->frame_size;
    uint16_t rx_buf_num = 0;
    memif_buffer_t* rx_bufs = rx_ctx->shm_bufs;

    if (rx_ctx->shm_ready == 0) {
        ERROR("MemIF connection not ready.");
        return -1;
    }

    err = memif_buffer_alloc(rx_ctx->memif_conn, qid, rx_bufs, buf_num, &rx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("Failed to alloc memif buffer: %s", memif_strerror(err));
        return -1;
    }

    ext_frame->addr[0] = rx_bufs->data;
    // ext_frame->buf_iova = rx_ctx->frames_begin_iova + ((uint8_t*)rx_bufs->data - (uint8_t*)rx_ctx->frames_begin_addr);
    ext_frame->iova[0] = rx_ctx->source_begin_iova + ((uint8_t*)rx_bufs->data - rx_ctx->source_begin);
    ext_frame->size = rx_bufs->len;

    /* save your private data here get it from st_frame.opaque */
    ext_frame->opaque = rx_bufs;

    return 0;
}

static int rx_st22p_query_ext_frame(void* priv, struct st_ext_frame* ext_frame,
    struct st22_rx_frame_meta* meta)
{
    rx_st22p_session_context_t * rx_ctx = priv;
    int err = 0;

    /* allocate memory */
    uint16_t qid = 0;
    uint16_t buf_num = 1;
    uint32_t buf_size = rx_ctx->frame_size;
    uint16_t rx_buf_num = 0;
    int width = rx_ctx->width, height = rx_ctx->height;
    memif_buffer_t* rx_bufs = rx_ctx->shm_bufs;

    if (rx_ctx->shm_ready == 0) {
        ERROR("MemIF connection not ready.");
        return -1;
    }

    err = memif_buffer_alloc(rx_ctx->memif_conn, qid, rx_bufs, buf_num, &rx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("Failed to alloc memif buffer: %s", memif_strerror(err));
        return -1;
    }

    uint8_t planes = st_frame_fmt_planes(rx_ctx->output_fmt);
    size_t frame_size = rx_ctx->frame_size;
    size_t pg_sz = mtl_page_size(rx_ctx->st);
    rx_ctx->ext_fb_malloc = rx_bufs->data;
    rx_ctx->ext_fb = (uint8_t*)MTL_ALIGN((uint64_t)rx_ctx->ext_fb_malloc, pg_sz);
    rx_ctx->ext_fb_iova = rx_ctx->source_begin_iova + ((uint8_t*)rx_bufs->data - rx_ctx->source_begin);

    for (uint8_t plane = 0; plane < planes; plane++) { /* assume planes continuous */
        ext_frame->linesize[plane] = st_frame_least_linesize(rx_ctx->output_fmt, width, plane);
        if (plane == 0) {
            ext_frame->addr[plane] = rx_bufs->data;
            ext_frame->iova[plane] = rx_ctx->ext_fb_iova;
        } else {
            ext_frame->addr[plane] = (uint8_t*)ext_frame->addr[plane - 1] + ext_frame->linesize[plane - 1] * height;
            ext_frame->iova[plane] = ext_frame->iova[plane - 1] + ext_frame->linesize[plane - 1] * height;
        }
    }
    ext_frame->size = frame_size;

    /* save your private data here get it from st_frame.opaque */
    ext_frame->opaque = rx_bufs;

    return 0;
}
#endif

static int tx_st20p_frame_available(void* priv)
{
    tx_session_context_t* s = (tx_session_context_t*)priv;

    st_pthread_mutex_lock(&s->wake_mutex);
    st_pthread_cond_signal(&s->wake_cond);
    st_pthread_mutex_unlock(&s->wake_mutex);

    return 0;
}

static int tx_st22p_frame_available(void* priv)
{
    tx_st22p_session_context_t* s = (tx_st22p_session_context_t*)priv;

    st_pthread_mutex_lock(&s->st22p_wake_mutex);
    st_pthread_cond_signal(&s->st22p_wake_cond);
    st_pthread_mutex_unlock(&s->st22p_wake_mutex);

    return 0;
}

/* Monotonic time (in nanoseconds) since some unspecified starting point. */
static inline uint64_t st_app_get_monotonic_time()
{
    struct timespec ts;

    clock_gettime(ST_CLOCK_MONOTONIC_ID, &ts);
    return ((uint64_t)ts.tv_sec * NS_PER_S) + ts.tv_nsec;
}

static int rx_st30_frame_ready(void* priv, void* frame,
    struct st30_rx_frame_meta* meta)
{
    rx_st30_session_context_t* s = priv;

    if (!s->handle)
        return -1;

    s->stat_frame_total_received++;
    if (!s->stat_frame_first_rx_time)
        s->stat_frame_first_rx_time = st_app_get_monotonic_time();

    rx_st30_consume_frame(s, frame);

    st30_rx_put_framebuff(s->handle, frame);

    return 0;
}

static int rx_st30_rtp_ready(void* priv)
{
    rx_st30_session_context_t* s = priv;

    st_pthread_mutex_lock(&s->st30_wake_mutex);
    st_pthread_cond_signal(&s->st30_wake_cond);
    st_pthread_mutex_unlock(&s->st30_wake_mutex);

    return 0;
}

static int rx_st40_rtp_ready(void* priv)
{
    rx_st40_session_context_t* s = priv;

    st_pthread_mutex_lock(&s->st40_wake_mutex);
    st_pthread_cond_signal(&s->st40_wake_cond);
    st_pthread_mutex_unlock(&s->st40_wake_mutex);

    return 0;
}

static int tx_st20p_frame_done(void* priv, struct st_frame* frame)
{
    int err = 0;
    memif_conn_handle_t conn = NULL;
    uint16_t qid = 0;
    uint16_t buf_num = 1;
    tx_session_context_t* tx_ctx = (tx_session_context_t*)priv;

    if (tx_ctx == NULL || tx_ctx->handle == NULL) {
        return -1; /* not ready */
    }

    // conn = tx_ctx->memif_conn;
    conn = (memif_conn_handle_t)frame->opaque;
    if (conn == NULL) {
        return -1;
    }

    /* return back frame buffer to memif. */
    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    return err;
}

static int tx_st22p_frame_done(void* priv, struct st_frame* frame)
{
    int err = 0;
    memif_conn_handle_t conn = NULL;
    uint16_t qid = 0;
    uint16_t buf_num = 1;
    tx_st22p_session_context_t* tx_ctx = (tx_st22p_session_context_t*)priv;

    if (tx_ctx == NULL || tx_ctx->handle == NULL) {
        return -1; /* not ready */
    }

    conn = (memif_conn_handle_t)frame->opaque;
    if (conn == NULL) {
        return -1;
    }

    /* return back frame buffer to memif. */
    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    return err;
}

static void rx_st20p_consume_frame(rx_session_context_t* s, struct st_frame* frame)
{
    int err = 0;
    uint16_t qid = 0;
    memif_buffer_t* rx_bufs = NULL;
    uint16_t buf_num = 1;
    memif_conn_handle_t conn;
    uint32_t buf_size = s->frame_size;
    uint16_t rx_buf_num = 0, rx = 0;

    if (s->shm_ready == 0) {
        INFO("%s memif not ready\n", __func__);
        return;
    }

    conn = s->memif_conn;

#if defined(ZERO_COPY)
    rx_bufs = (memif_buffer_t*)frame->opaque;
    rx_buf_num = 1;
#else
    /* allocate memory */
    rx_bufs = s->shm_bufs;

    err = memif_buffer_alloc(s->memif_conn, qid, rx_bufs, buf_num, &rx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("Failed to alloc memif buffer: %s", memif_strerror(err));
        return;
    }

    for (int i = 0; i < rx_buf_num; i++) {
        mtl_memcpy(rx_bufs->data, frame->addr[0], s->frame_size);
    }
#endif

    /* Send to microservice application. */
    err = memif_tx_burst(s->memif_conn, qid, rx_bufs, rx_buf_num, &rx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_tx_burst: %s", memif_strerror(err));
    }

    s->fb_recv++;
}

static void rx_st22p_consume_frame(rx_st22p_session_context_t* s, struct st_frame* frame)
{
    int err = 0;
    uint16_t qid = 0;
    memif_buffer_t* rx_bufs = NULL;
    uint16_t buf_num = 1;
    memif_conn_handle_t conn;
    uint32_t buf_size = s->frame_size;
    uint16_t rx_buf_num = 0, rx = 0;

    if (s->shm_ready == 0) {
        INFO("%s memif not ready\n", __func__);
        return;
    }

    conn = s->memif_conn;

#if defined(ZERO_COPY)
    rx_bufs = (memif_buffer_t*)frame->opaque;
    rx_buf_num = 1;
#else
    /* allocate memory */
    rx_bufs = s->shm_bufs;

    err = memif_buffer_alloc(s->memif_conn, qid, rx_bufs, buf_num, &rx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("Failed to alloc memif buffer: %s", memif_strerror(err));
        return;
    }

    // void* testdata = malloc(s->frame_size);
    // mtl_memcpy(rx_bufs->data, testdata, s->frame_size);
    // mtl_memcpy(testdata, frame->addr, s->frame_size);

    uint8_t planes = st_frame_fmt_planes(frame->fmt);
    uint8_t* dst = rx_bufs->data;
    for (uint8_t plane = 0; plane < planes; plane++) {
        size_t plane_sz = st_frame_plane_size(frame, plane);
        mtl_memcpy(dst, frame->addr[plane], plane_sz);
        dst += plane_sz;
    }
#endif

    // mtl_memcpy(rx_bufs->data, frame->addr, s->frame_size);

    /* Send to microservice application. */
    err = memif_tx_burst(s->memif_conn, qid, rx_bufs, rx_buf_num, &rx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_tx_burst: %s", memif_strerror(err));
    }

    s->fb_recv++;
}

static void rx_st30_consume_frame(rx_st30_session_context_t* s, void* frame)
{
    int err = 0;
    uint16_t qid = 0;
    memif_buffer_t* tx_bufs = NULL;
    uint16_t buf_num = 1;
    memif_conn_handle_t conn;
    uint32_t buf_size = s->pkt_len;
    uint16_t tx_buf_num = 0, tx = 0;

    if (s->shm_ready == 0) {
        INFO("%s memif not ready\n", __func__);
        return;
    }

    conn = s->memif_conn;

    /* allocate memory */
    tx_bufs = s->shm_bufs;

    err = memif_buffer_alloc(s->memif_conn, qid, tx_bufs, buf_num, &tx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("Failed to alloc memif buffer: %s", memif_strerror(err));
        return;
    }

    /* Fill the shared-memory buffer. */
    uint8_t* dst = tx_bufs->data;
    mtl_memcpy(dst, frame, s->st30_frame_size);

    /* Send to microservice application. */
    err = memif_tx_burst(s->memif_conn, qid, tx_bufs, tx_buf_num, &tx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_tx_burst: %s", memif_strerror(err));
    }

    return;
}

static void rx_st40_consume_frame(rx_st40_session_context_t* s, void* usrptr, uint16_t len)
{
    int err = 0;
    uint16_t qid = 0;
    memif_buffer_t* tx_bufs = NULL;
    uint16_t buf_num = 1;
    memif_conn_handle_t conn;
    uint32_t buf_size = s->pkt_len;
    uint16_t tx_buf_num = 0, tx = 0;

    if (s->shm_ready == 0) {
        INFO("%s memif not ready\n", __func__);
        return;
    }

    conn = s->memif_conn;

    /* allocate memory */
    tx_bufs = s->shm_bufs;

    err = memif_buffer_alloc(s->memif_conn, qid, tx_bufs, buf_num, &tx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("Failed to alloc memif buffer: %s", memif_strerror(err));
        return;
    }

    /* Fill the shared-memory buffer. */
    uint8_t* dst = tx_bufs->data;
    mtl_memcpy(dst, usrptr, len);

    /* Send to microservice application. */
    err = memif_tx_burst(s->memif_conn, qid, tx_bufs, tx_buf_num, &tx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_tx_burst: %s", memif_strerror(err));
    }

    return;
}

static void rx_st40_handle_rtp(rx_st40_session_context_t* s, void* usrptr)
{
    struct st40_rfc8331_rtp_hdr* hdr = (struct st40_rfc8331_rtp_hdr*)usrptr;
    struct st40_rfc8331_payload_hdr* payload_hdr = (struct st40_rfc8331_payload_hdr*)(&hdr[1]);
    int anc_count = hdr->anc_count;
    int idx, total_size, payload_len;

    for (idx = 0; idx < anc_count; idx++) {
        payload_hdr->swaped_first_hdr_chunk = ntohl(payload_hdr->swaped_first_hdr_chunk);
        payload_hdr->swaped_second_hdr_chunk = ntohl(payload_hdr->swaped_second_hdr_chunk);
        if (!st40_check_parity_bits(payload_hdr->second_hdr_chunk.did) || !st40_check_parity_bits(payload_hdr->second_hdr_chunk.sdid) || !st40_check_parity_bits(payload_hdr->second_hdr_chunk.data_count)) {
            ERROR("anc RTP checkParityBits error\n");
            return;
        }
        int udw_size = payload_hdr->second_hdr_chunk.data_count & 0xff;

        // verify checksum
        uint16_t checksum = 0;
        checksum = st40_get_udw(udw_size + 3, (uint8_t*)&payload_hdr->second_hdr_chunk);
        payload_hdr->swaped_second_hdr_chunk = htonl(payload_hdr->swaped_second_hdr_chunk);
        if (checksum != st40_calc_checksum(3 + udw_size, (uint8_t*)&payload_hdr->second_hdr_chunk)) {
            ERROR("anc frame checksum error\n");
            return;
        }
        // get payload
#ifdef DEBUG
        uint16_t data;
        for (int i = 0; i < udw_size; i++) {
            data = st40_get_udw(i + 3, (uint8_t*)&payload_hdr->second_hdr_chunk);
            if (!st40_check_parity_bits(data))
                ERROR("anc udw checkParityBits error\n");
            DEBUG("%c", data & 0xff);
        }
        DEBUG("\n");
#endif
        total_size = ((3 + udw_size + 1) * 10) / 8; // Calculate size of the
                                                    // 10-bit words: DID, SDID, DATA_COUNT
                                                    // + size of buffer with data + checksum
        total_size = (4 - total_size % 4) + total_size; // Calculate word align to the 32-bit
                                                        // word of ANC data packet
        payload_len = sizeof(struct st40_rfc8331_payload_hdr) - 4 + total_size; // Full size of one ANC
        payload_hdr = (struct st40_rfc8331_payload_hdr*)((uint8_t*)payload_hdr + payload_len);
    }

    s->stat_frame_total_received++;
    if (!s->stat_frame_first_rx_time)
        s->stat_frame_first_rx_time = st_app_get_monotonic_time();
}

static void* rx_st20p_frame_thread(void* arg)
{
    rx_session_context_t* s = (rx_session_context_t*)arg;
    st20p_rx_handle handle = s->handle;
    struct st_frame* frame;

    printf("%s(%d), start\n", __func__, s->idx);
    while (!s->stop) {
        frame = st20p_rx_get_frame(handle);
        if (!frame) { /* no frame */
            st_pthread_mutex_lock(&s->wake_mutex);
            if (!s->stop)
                st_pthread_cond_wait(&s->wake_cond, &s->wake_mutex);
            st_pthread_mutex_unlock(&s->wake_mutex);
            continue;
        }
        /* Verify frame data status. */
        if (frame->status == ST_FRAME_STATUS_CORRUPTED) {
            printf("[DBG] Received corrupted frame.\n");
        } else {
            rx_st20p_consume_frame(s, frame);
        }
        st20p_rx_put_frame(handle, frame);
    }

    return NULL;
}

static void* rx_st22p_frame_thread(void* arg)
{
    rx_st22p_session_context_t* s = (rx_st22p_session_context_t*)arg;
    st22p_rx_handle handle = s->handle;
    struct st_frame* frame;

    printf("%s(%d), start\n", __func__, s->idx);
    while (!s->stop) {
        frame = st22p_rx_get_frame(handle);
        if (!frame) { /* no frame */
            st_pthread_mutex_lock(&s->st22p_wake_mutex);
            if (!s->stop)
                st_pthread_cond_wait(&s->st22p_wake_cond, &s->st22p_wake_mutex);
            st_pthread_mutex_unlock(&s->st22p_wake_mutex);
            continue;
        }
        /* Verify frame data status. */
        if (frame->status == ST_FRAME_STATUS_CORRUPTED) {
            printf("[DBG] Received corrupted frame.\n");
        } else {
            rx_st22p_consume_frame(s, frame);
        }
        st22p_rx_put_frame(handle, frame);
    }

    return NULL;
}

static void* rx_st40_frame_thread(void* arg)
{
    rx_st40_session_context_t* s = arg;
    int idx = s->idx;
    void* usrptr = NULL;
    uint16_t len = 0;
    void* mbuf = NULL;

    INFO("%s(%d), start\n", __func__, idx);
    while (!s->stop) {
        mbuf = st40_rx_get_mbuf(s->handle, &usrptr, &len);
        if (!mbuf) {
            /* no buffer */
            st_pthread_mutex_lock(&s->st40_wake_mutex);
            if (!s->stop)
                st_pthread_cond_wait(&s->st40_wake_cond, &s->st40_wake_mutex);
            st_pthread_mutex_unlock(&s->st40_wake_mutex);
            continue;
        }
        /* parse the packet */
        // rx_st40_handle_rtp(s, usrptr);
        rx_st40_consume_frame(s, usrptr, len);
        st40_rx_put_mbuf(s->handle, mbuf);
    }
    INFO("%s(%d), stop\n", __func__, idx);

    return NULL;
}

/* Initiliaze IMTL library */
mtl_handle inst_init(struct mtl_init_params* st_param)
{
    mtl_handle dev_handle = NULL;
    struct mtl_init_params param = { 0 };

    if (st_param == NULL) {
        int session_num = 1;
        const char port_bdf[] = "0000:31:00.0";
        const uint8_t local_ip[MTL_IP_ADDR_LEN] = { 192, 168, 96, 1 };

        /* set default parameters */
        param.num_ports = 1;
        strncpy(param.port[MTL_PORT_P], port_bdf, MTL_PORT_MAX_LEN);
        memcpy(param.sip_addr[MTL_PORT_P], local_ip, MTL_IP_ADDR_LEN);
        param.flags = MTL_FLAG_TASKLET_THREAD;
        param.log_level = MTL_LOG_LEVEL_INFO; // log level. ERROR, INFO, WARNING
        param.priv = NULL; // usr crx pointer
        param.ptp_get_time_fn = NULL;
        param.rx_queues_cnt[MTL_PORT_P] = session_num;
        param.tx_queues_cnt[MTL_PORT_P] = session_num;
        param.lcores = NULL;
    } else {
        mtl_memcpy(&param, st_param, sizeof(struct mtl_init_params));
        param.flags |= MTL_FLAG_RX_UDP_PORT_ONLY;
    }

    // create device
    dev_handle = mtl_init(&param);
    if (!dev_handle) {
        printf("%s, st_init fail\n", __func__);
        return NULL;
    }

    // start MTL device
    if (mtl_start(dev_handle) != 0) {
        INFO("%s, Fail to start MTL device.", __func__);
        return NULL;
    }

    return dev_handle;
}

static void* tx_memif_event_loop(void* arg)
{
    int err = MEMIF_ERR_SUCCESS;
    memif_socket_handle_t memif_socket = (memif_socket_handle_t)arg;

    do {
        // INFO("media-proxy waiting event.");
        err = memif_poll_event(memif_socket, -1);
        // INFO("media-proxy received event.");
    } while (err == MEMIF_ERR_SUCCESS);

    INFO("MEMIF DISCONNECTED.");

    return NULL;
}

static void* rx_memif_event_loop(void* arg)
{
    int err = MEMIF_ERR_SUCCESS;
    memif_socket_handle_t memif_socket = (memif_socket_handle_t)arg;

    do {
        // INFO("media-proxy waiting event.");
        err = memif_poll_event(memif_socket, -1);
        // INFO("media-proxy received event.");
    } while (err == MEMIF_ERR_SUCCESS);

    INFO("MEMIF DISCONNECTED.");

    return NULL;
}

int rx_st20p_shm_init(rx_session_context_t* rx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    memif_ops_t default_memif_ops = { 0 };

    if (rx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(rx_ctx->memif_socket_args.app_name, sizeof(rx_ctx->memif_socket_args.app_name));
    bzero(rx_ctx->memif_socket_args.path, sizeof(rx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_rx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_rx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_rx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(rx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(rx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(rx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(rx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(rx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&rx_ctx->memif_socket, &rx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    rx_ctx->shm_ready = 0;
    rx_ctx->memif_conn_args.socket = rx_ctx->memif_socket;
    rx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    rx_ctx->memif_conn_args.buffer_size = (uint32_t)rx_ctx->frame_size;
    rx_ctx->memif_conn_args.log2_ring_size = 2;
    memcpy((char*)rx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(rx_ctx->memif_conn_args.interface_name));
    rx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    INFO("create memif interface.");
    ret = memif_create(&rx_ctx->memif_conn, &rx_ctx->memif_conn_args,
        rx_st20p_on_connect, rx_st20p_on_disconnect, rx_on_receive, rx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&rx_ctx->memif_event_thread, NULL, rx_memif_event_loop, rx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        return -1;
    }

    return 0;
}

int rx_st22p_shm_init(rx_st22p_session_context_t* rx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    memif_ops_t default_memif_ops = { 0 };

    if (rx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(rx_ctx->memif_socket_args.app_name, sizeof(rx_ctx->memif_socket_args.app_name));
    bzero(rx_ctx->memif_socket_args.path, sizeof(rx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_rx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_rx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_rx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(rx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(rx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(rx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(rx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(rx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&rx_ctx->memif_socket, &rx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    rx_ctx->shm_ready = 0;
    rx_ctx->memif_conn_args.socket = rx_ctx->memif_socket;
    rx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    rx_ctx->memif_conn_args.buffer_size = (uint32_t)rx_ctx->frame_size;
    rx_ctx->memif_conn_args.log2_ring_size = 2;
    memcpy((char*)rx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(rx_ctx->memif_conn_args.interface_name));
    rx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    INFO("create memif interface.");
    ret = memif_create(&rx_ctx->memif_conn, &rx_ctx->memif_conn_args,
        rx_st22p_on_connect, rx_st22p_on_disconnect, rx_on_receive, rx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&rx_ctx->memif_event_thread, NULL, rx_memif_event_loop, rx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        return -1;
    }

    return 0;
}

int rx_shm_deinit(rx_session_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(rx_ctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&rx_ctx->memif_conn);
    memif_delete_socket(&rx_ctx->memif_socket);

    /* unlink socket file */
    if (rx_ctx->memif_conn_args.is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        unlink(rx_ctx->memif_socket_args.path);
    }

    if (rx_ctx->shm_bufs) {
        free(rx_ctx->shm_bufs);
        rx_ctx->shm_bufs = NULL;
    }

    return 0;
}

int tx_shm_deinit(tx_session_context_t* tx_ctx)
{
    if (tx_ctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(tx_ctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&tx_ctx->memif_conn);
    memif_delete_socket(&tx_ctx->memif_socket);

    /* unlink socket file */
    if (tx_ctx->memif_conn_args.is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        unlink(tx_ctx->memif_socket_args.path);
    }

    if (tx_ctx->shm_bufs) {
        free(tx_ctx->shm_bufs);
        tx_ctx->shm_bufs = NULL;
    }

    return 0;
}

int rx_st22p_shm_deinit(rx_st22p_session_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(rx_ctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&rx_ctx->memif_conn);
    memif_delete_socket(&rx_ctx->memif_socket);

    /* unlink socket file */
    if (rx_ctx->memif_conn_args.is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        unlink(rx_ctx->memif_socket_args.path);
    }

    if (rx_ctx->shm_bufs) {
        free(rx_ctx->shm_bufs);
        rx_ctx->shm_bufs = NULL;
    }

    return 0;
}

int tx_st22p_shm_deinit(tx_st22p_session_context_t* tx_ctx)
{
    if (tx_ctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    // pthread_cancel(tx_ctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&tx_ctx->memif_conn);
    memif_delete_socket(&tx_ctx->memif_socket);

    /* unlink socket file */
    if (tx_ctx->memif_conn_args.is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        unlink(tx_ctx->memif_socket_args.path);
    }

    if (tx_ctx->shm_bufs) {
        free(tx_ctx->shm_bufs);
        tx_ctx->shm_bufs = NULL;
    }

    return 0;
}

int rx_st30_shm_deinit(rx_st30_session_context_t* pctx)
{
    if (pctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(pctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&pctx->memif_conn);
    memif_delete_socket(&pctx->memif_socket);

    /* unlink socket file */
    if (pctx->memif_conn_args.is_master && pctx->memif_socket_args.path[0] != '@') {
        unlink(pctx->memif_socket_args.path);
    }

    if (pctx->shm_bufs) {
        free(pctx->shm_bufs);
        pctx->shm_bufs = NULL;
    }

    return 0;
}

int tx_st30_shm_deinit(tx_st30_session_context_t* pctx)
{
    if (pctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(pctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&pctx->memif_conn);
    memif_delete_socket(&pctx->memif_socket);

    /* unlink socket file */
    if (pctx->memif_conn_args.is_master && pctx->memif_socket_args.path[0] != '@') {
        unlink(pctx->memif_socket_args.path);
    }

    if (pctx->shm_bufs) {
        free(pctx->shm_bufs);
        pctx->shm_bufs = NULL;
    }

    if (pctx->framebuffs) {
        free(pctx->shm_bufs);
        pctx->shm_bufs = NULL;
    }

    return 0;
}

int rx_st40_shm_deinit(rx_st40_session_context_t* pctx)
{
    if (pctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(pctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&pctx->memif_conn);
    memif_delete_socket(&pctx->memif_socket);

    /* unlink socket file */
    if (pctx->memif_conn_args.is_master && pctx->memif_socket_args.path[0] != '@') {
        unlink(pctx->memif_socket_args.path);
    }

    if (pctx->shm_bufs) {
        free(pctx->shm_bufs);
        pctx->shm_bufs = NULL;
    }

    return 0;
}

int tx_st40_shm_deinit(tx_st40_session_context_t* pctx)
{
    if (pctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(pctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&pctx->memif_conn);
    memif_delete_socket(&pctx->memif_socket);

    /* unlink socket file */
    if (pctx->memif_conn_args.is_master && pctx->memif_socket_args.path[0] != '@') {
        unlink(pctx->memif_socket_args.path);
    }

    if (pctx->shm_bufs) {
        free(pctx->shm_bufs);
        pctx->shm_bufs = NULL;
    }

    if (pctx->framebuffs) {
        free(pctx->shm_bufs);
        pctx->shm_bufs = NULL;
    }

    return 0;
}

int tx_st20p_shm_init(tx_session_context_t* tx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    const uint16_t FRAME_COUNT = 4;
    memif_ops_t default_memif_ops = { 0 };

    if (tx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(tx_ctx->memif_socket_args.app_name, sizeof(tx_ctx->memif_socket_args.app_name));
    bzero(tx_ctx->memif_socket_args.path, sizeof(tx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_tx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_tx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_tx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(tx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(tx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(tx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(tx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(tx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&tx_ctx->memif_socket, &tx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    tx_ctx->shm_ready = 0;
    tx_ctx->memif_conn_args.socket = tx_ctx->memif_socket;
    tx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    tx_ctx->memif_conn_args.buffer_size = (uint32_t)tx_ctx->frame_size;
    tx_ctx->memif_conn_args.log2_ring_size = 2;
    snprintf((char*)tx_ctx->memif_conn_args.interface_name,
        sizeof(tx_ctx->memif_conn_args.interface_name), "%s", memif_ops->interface_name);
    tx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    /* TX buffers */
    tx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * FRAME_COUNT);
    tx_ctx->shm_buf_num = FRAME_COUNT;

    INFO("Create memif interface.");
    ret = memif_create(&tx_ctx->memif_conn, &tx_ctx->memif_conn_args,
        tx_st20p_on_connect, tx_st20p_on_disconnect, tx_st20p_on_receive, tx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        free(tx_ctx->shm_bufs);
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&tx_ctx->memif_event_thread, NULL, tx_memif_event_loop, tx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        free(tx_ctx->shm_bufs);
        return -1;
    }

    return 0;
}

int tx_st22p_shm_init(tx_st22p_session_context_t* tx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    const uint16_t FRAME_COUNT = 4;
    memif_ops_t default_memif_ops = { 0 };

    if (tx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(tx_ctx->memif_socket_args.app_name, sizeof(tx_ctx->memif_socket_args.app_name));
    bzero(tx_ctx->memif_socket_args.path, sizeof(tx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_tx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_tx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_tx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(tx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(tx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(tx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(tx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(tx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&tx_ctx->memif_socket, &tx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    tx_ctx->shm_ready = 0;
    tx_ctx->memif_conn_args.socket = tx_ctx->memif_socket;
    tx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    tx_ctx->memif_conn_args.buffer_size = (uint32_t)tx_ctx->frame_size;
    tx_ctx->memif_conn_args.log2_ring_size = 2;
    memcpy((char*)tx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(tx_ctx->memif_conn_args.interface_name));
    tx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    /* TX buffers */
    tx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * FRAME_COUNT);
    tx_ctx->shm_buf_num = FRAME_COUNT;

    INFO("Create memif interface.");
    ret = memif_create(&tx_ctx->memif_conn, &tx_ctx->memif_conn_args,
        tx_st22p_on_connect, tx_st22p_on_disconnect, tx_st22p_on_receive, tx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        free(tx_ctx->shm_bufs);
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&tx_ctx->memif_event_thread, NULL, tx_memif_event_loop, tx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        free(tx_ctx->shm_bufs);
        return -1;
    }

    return 0;
}

int tx_st30_shm_init(tx_st30_session_context_t* tx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    memif_ops_t default_memif_ops = { 0 };

    if (tx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(tx_ctx->memif_socket_args.app_name, sizeof(tx_ctx->memif_socket_args.app_name));
    bzero(tx_ctx->memif_socket_args.path, sizeof(tx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_tx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_tx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_tx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(tx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(tx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(tx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(tx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(tx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&tx_ctx->memif_socket, &tx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    tx_ctx->shm_ready = 0;
    tx_ctx->memif_conn_args.socket = tx_ctx->memif_socket;
    tx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    tx_ctx->memif_conn_args.buffer_size = (uint32_t)tx_ctx->pkt_len;
    tx_ctx->memif_conn_args.log2_ring_size = 4;
    memcpy((char*)tx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(tx_ctx->memif_conn_args.interface_name));
    tx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    INFO("Create memif interface.");
    ret = memif_create(&tx_ctx->memif_conn, &tx_ctx->memif_conn_args,
        tx_st30_on_connect, tx_on_disconnect, tx_st30_on_receive, tx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&tx_ctx->memif_event_thread, NULL, tx_memif_event_loop, tx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        return -1;
    }

    return 0;
}

int rx_st30_shm_init(rx_st30_session_context_t* rx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    memif_ops_t default_memif_ops = { 0 };

    if (rx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(rx_ctx->memif_socket_args.app_name, sizeof(rx_ctx->memif_socket_args.app_name));
    bzero(rx_ctx->memif_socket_args.path, sizeof(rx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_rx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_rx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_rx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(rx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(rx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(rx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(rx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(rx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&rx_ctx->memif_socket, &rx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    rx_ctx->shm_ready = 0;
    rx_ctx->memif_conn_args.socket = rx_ctx->memif_socket;
    rx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    rx_ctx->memif_conn_args.buffer_size = (uint32_t)rx_ctx->pkt_len;
    rx_ctx->memif_conn_args.log2_ring_size = 2;
    memcpy((char*)rx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(rx_ctx->memif_conn_args.interface_name));
    rx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    INFO("Create memif interface.");
    ret = memif_create(&rx_ctx->memif_conn, &rx_ctx->memif_conn_args,
        rx_st30_on_connect, rx_on_disconnect, rx_on_receive, rx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&rx_ctx->memif_event_thread, NULL, rx_memif_event_loop, rx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        return -1;
    }

    return 0;
}

int tx_st40_shm_init(tx_st40_session_context_t* tx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    const uint16_t FRAME_COUNT = 4;
    memif_ops_t default_memif_ops = { 0 };

    if (tx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(tx_ctx->memif_socket_args.app_name, sizeof(tx_ctx->memif_socket_args.app_name));
    bzero(tx_ctx->memif_socket_args.path, sizeof(tx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_tx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_tx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_tx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(tx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(tx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(tx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(tx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(tx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&tx_ctx->memif_socket, &tx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    tx_ctx->shm_ready = 0;
    tx_ctx->memif_conn_args.socket = tx_ctx->memif_socket;
    tx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    tx_ctx->memif_conn_args.buffer_size = (uint32_t)tx_ctx->pkt_len;
    tx_ctx->memif_conn_args.log2_ring_size = 2;
    memcpy((char*)tx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(tx_ctx->memif_conn_args.interface_name));
    tx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    /* TX buffers */
    tx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * FRAME_COUNT);
    tx_ctx->shm_buf_num = FRAME_COUNT;

    INFO("Create memif interface.");
    ret = memif_create(&tx_ctx->memif_conn, &tx_ctx->memif_conn_args,
        tx_st40_on_connect, tx_on_disconnect, tx_st40_on_receive, tx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        free(tx_ctx->shm_bufs);
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&tx_ctx->memif_event_thread, NULL, tx_memif_event_loop, tx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        free(tx_ctx->shm_bufs);
        return -1;
    }

    return 0;
}

int rx_st40_shm_init(rx_st40_session_context_t* rx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    memif_ops_t default_memif_ops = { 0 };

    if (rx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(rx_ctx->memif_socket_args.app_name, sizeof(rx_ctx->memif_socket_args.app_name));
    bzero(rx_ctx->memif_socket_args.path, sizeof(rx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_rx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_rx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_rx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(rx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(rx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(rx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(rx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(rx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&rx_ctx->memif_socket, &rx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    rx_ctx->shm_ready = 0;
    rx_ctx->memif_conn_args.socket = rx_ctx->memif_socket;
    rx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    rx_ctx->memif_conn_args.buffer_size = (uint32_t)rx_ctx->pkt_len;
    rx_ctx->memif_conn_args.log2_ring_size = 2;
    memcpy((char*)rx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(rx_ctx->memif_conn_args.interface_name));
    rx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    INFO("Create memif interface.");
    ret = memif_create(&rx_ctx->memif_conn, &rx_ctx->memif_conn_args,
        rx_st40_on_connect, rx_on_disconnect, rx_on_receive, rx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&rx_ctx->memif_event_thread, NULL, rx_memif_event_loop, rx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        return -1;
    }

    return 0;
}

/* Create new RX session */
rx_session_context_t* mtl_st20p_rx_session_create(mtl_handle dev_handle, struct st20p_rx_ops* opts, memif_ops_t* memif_ops)
{
    int ret = 0;
    static int idx = 0;
    int fb_cnt = 4;
    rx_session_context_t* rx_ctx = NULL;

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    rx_ctx = calloc(1, sizeof(rx_session_context_t));
    if (rx_ctx == NULL) {
        printf("%s, RX session contex malloc fail\n", __func__);
        return NULL;
    }

    rx_ctx->st = dev_handle;
    rx_ctx->idx = idx;
    rx_ctx->stop = false;
    st_pthread_mutex_init(&rx_ctx->wake_mutex, NULL);
    st_pthread_cond_init(&rx_ctx->wake_cond, NULL);

    struct st20p_rx_ops ops_rx = { 0 };
    if (opts == NULL) { /* set parameters to default */
        const char RX_ST20_PORT_BDF[] = "0000:31:00.1";
        const uint16_t RX_ST20_UDP_PORT = 20000 + idx;
        const uint8_t RX_ST20_PAYLOAD_TYPE = 112;
        /* source ip address for rx session */
        const static uint8_t g_rx_st20_src_ip[MTL_IP_ADDR_LEN] = { 192, 168, 96, 1 };

        ops_rx.name = "mcm_rx_session";
        ops_rx.port.num_port = 1;
        // rx src ip like 239.0.0.1
        memcpy(ops_rx.port.sip_addr[MTL_PORT_P], g_rx_st20_src_ip, MTL_IP_ADDR_LEN);
        // send port interface like 0000:af:00.0
        strncpy(ops_rx.port.port[MTL_PORT_P], RX_ST20_PORT_BDF, MTL_PORT_MAX_LEN);
        ops_rx.port.payload_type = RX_ST20_PAYLOAD_TYPE;
        ops_rx.width = 1920;
        ops_rx.height = 1080;
        ops_rx.fps = ST_FPS_P60;
        ops_rx.transport_fmt = ST20_FMT_YUV_422_10BIT;
        ops_rx.output_fmt = ST_FRAME_FMT_YUV444PLANAR10LE;
        ops_rx.device = ST_PLUGIN_DEVICE_AUTO;
        ops_rx.port.udp_port[MTL_PORT_P] = RX_ST20_UDP_PORT;
        ops_rx.framebuff_cnt = fb_cnt;
    } else {
        mtl_memcpy(&ops_rx, opts, sizeof(struct st20p_rx_ops));
    }

    ops_rx.priv = rx_ctx; // app handle register to lib
    ops_rx.notify_frame_available = rx_st20p_frame_available;

    rx_ctx->frame_size = st20_frame_size(ops_rx.transport_fmt, ops_rx.width, ops_rx.height);

    /* initialize share memory */
    ret = rx_st20p_shm_init(rx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        st_pthread_mutex_destroy(&rx_ctx->wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->wake_cond);
        free(rx_ctx);
        return NULL;
    }

    /* Waiting for memif connection created. */
    // printf("Waiting for consumer service connection.\n");
    // while (!rx_ctx->shm_ready) {
    //     sleep(1);
    // }
    fb_cnt = rx_ctx->fb_count;

#if defined(ZERO_COPY)
    rx_ctx->ext_frames = (struct st20_ext_frame*)malloc(sizeof(*rx_ctx->ext_frames) * fb_cnt);
    // for (int i = 0; i < fb_cnt; i++) {
    //     rx_ctx->ext_frames[i].buf_addr = rx_ctx->frames_begin_addr + i * rx_ctx->frame_size;
    //     rx_ctx->ext_frames[i].buf_iova = rx_ctx->frames_begin_iova + i * rx_ctx->frame_size;
    //     rx_ctx->ext_frames[i].buf_len = rx_ctx->frame_size;
    // }
    // rx_ctx->ext_idx = 0;

    ops_rx.flags |= ST20P_RX_FLAG_EXT_FRAME;
    // ops_rx.ext_frames = rx_ctx->ext_frames;
    ops_rx.query_ext_frame = rx_st20p_query_ext_frame;
    ops_rx.flags |= ST20P_RX_FLAG_RECEIVE_INCOMPLETE_FRAME;
#endif

    /* dump out parameters for debugging. */
    st_rx_debug_dump(ops_rx);

    st20p_rx_handle rx_handle = st20p_rx_create(dev_handle, &ops_rx);
    if (!rx_handle) {
        printf("%s, st20p_rx_create fail\n", __func__);
        st_pthread_mutex_destroy(&rx_ctx->wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->wake_cond);
        free(rx_ctx);
        return NULL;
    }
    rx_ctx->handle = rx_handle;
    rx_ctx->frame_size = st20p_rx_frame_size(rx_handle);

    /* Start MTL session thread. */
    ret = pthread_create(&rx_ctx->frame_thread, NULL, rx_st20p_frame_thread, rx_ctx);
    if (ret < 0) {
        printf("%s(%d), thread create fail %d\n", __func__, ret, rx_ctx->idx);
        st_pthread_mutex_destroy(&rx_ctx->wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->wake_cond);
        free(rx_ctx);
        return NULL;
    }

    /* increase session index */
    idx++;

    return rx_ctx;
}

/* RX: Create ST22P session */
rx_st22p_session_context_t* mtl_st22p_rx_session_create(mtl_handle dev_handle, struct st22p_rx_ops* opts, memif_ops_t* memif_ops)
{
    int ret = 0;
    static int idx = 0;
    rx_st22p_session_context_t* rx_ctx = NULL;
    struct st22p_rx_ops ops_rx = { 0 };

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    rx_ctx = calloc(1, sizeof(rx_st22p_session_context_t));
    if (rx_ctx == NULL) {
        printf("%s, RX session contex malloc fail\n", __func__);
        return NULL;
    }

    rx_ctx->st = dev_handle;
    rx_ctx->idx = idx;
    rx_ctx->stop = false;
    rx_ctx->fb_idx = 0;

    st_pthread_mutex_init(&rx_ctx->st22p_wake_mutex, NULL);
    st_pthread_cond_init(&rx_ctx->st22p_wake_cond, NULL);

    mtl_memcpy(&ops_rx, opts, sizeof(struct st22p_rx_ops));

    ops_rx.priv = rx_ctx; // app handle register to lib
    ops_rx.notify_frame_available = rx_st22p_frame_available;

#if defined(ZERO_COPY)
    ops_rx.flags |= ST22P_RX_FLAG_EXT_FRAME;
    ops_rx.flags |= ST22P_RX_FLAG_RECEIVE_INCOMPLETE_FRAME;
    ops_rx.query_ext_frame = rx_st22p_query_ext_frame;
#endif

    /* dump out parameters for debugging. */
    st_rx_st22p_debug_dump(ops_rx);

    st22p_rx_handle rx_handle = st22p_rx_create(dev_handle, &ops_rx);
    if (!rx_handle) {
        printf("%s, st22p_rx_create fail\n", __func__);
        st_pthread_mutex_destroy(&rx_ctx->st22p_wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->st22p_wake_cond);
        return NULL;
    }
    rx_ctx->handle = rx_handle;
    rx_ctx->frame_size = st22p_rx_frame_size(rx_handle);
    rx_ctx->width = ops_rx.width;
    rx_ctx->height = ops_rx.height;
    rx_ctx->output_fmt = ops_rx.output_fmt;

    /* initialize share memory */
    ret = rx_st22p_shm_init(rx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        st_pthread_mutex_destroy(&rx_ctx->st22p_wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->st22p_wake_cond);
        free(rx_ctx);
        return NULL;
    }

    /* Waiting for memif connection created. */
    // printf("Waiting for consumer service connection.\n");
    // while (!rx_ctx->shm_ready) {
    //     sleep(1);
    // }

    /* Start MTL session thread. */
    ret = pthread_create(&rx_ctx->frame_thread, NULL, rx_st22p_frame_thread, rx_ctx);
    if (ret < 0) {
        printf("%s(%d), thread create fail %d\n", __func__, ret, rx_ctx->idx);
        st_pthread_mutex_destroy(&rx_ctx->st22p_wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->st22p_wake_cond);
        free(rx_ctx);
        return NULL;
    }

    /* increase session index */
    idx++;

    return rx_ctx;
}

/* TX: Create ST30 session */
tx_st30_session_context_t* mtl_st30_tx_session_create(mtl_handle dev_handle, struct st30_tx_ops* opts, memif_ops_t* memif_ops)
{
    int ret = 0;
    tx_st30_session_context_t* tx_ctx = NULL;
    struct st30_tx_ops ops_tx = {};
    static int idx = 0;
    const int fb_cnt = 4;

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    tx_ctx = calloc(1, sizeof(tx_st30_session_context_t));
    if (tx_ctx == NULL) {
        printf("%s, TX session contex malloc fail\n", __func__);
        return NULL;
    }

    tx_ctx->framebuff_cnt = 2;
    tx_ctx->framebuffs = (struct st_tx_frame*)calloc(1, sizeof(*tx_ctx->framebuffs) * tx_ctx->framebuff_cnt);
    if (!tx_ctx->framebuffs) {
        free(tx_ctx);
        return NULL;
    }

    for (uint16_t j = 0; j < tx_ctx->framebuff_cnt; j++) {
        tx_ctx->framebuffs[j].stat = ST_TX_FRAME_FREE;
        tx_ctx->framebuffs[j].lines_ready = 0;
    }

    tx_ctx->st = dev_handle;
    tx_ctx->idx = idx;
    tx_ctx->stop = false;
    st_pthread_mutex_init(&tx_ctx->st30_wake_mutex, NULL);
    st_pthread_cond_init(&tx_ctx->st30_wake_cond, NULL);

    mtl_memcpy(&ops_tx, opts, sizeof(struct st30_tx_ops));

    ops_tx.priv = tx_ctx; // app handle register to lib

    ops_tx.get_next_frame = tx_st30_next_frame;
    ops_tx.notify_frame_done = tx_st30_frame_done;
    ops_tx.notify_rtp_done = tx_st30_rtp_done;

    /* dump out parameters for debugging. */
    // st_tx_st30_debug_dump(ops_tx);

    tx_ctx->sampling = ops_tx.sampling;
    tx_ctx->pkt_len = st30_get_packet_size(ops_tx.fmt, ops_tx.ptime, ops_tx.sampling, ops_tx.channel);

    int pkt_per_frame = 1;

    double pkt_time = st30_get_packet_time(ops_tx.ptime);
    /* when ptime <= 1ms, set frame time to 1ms */
    if (pkt_time < NS_PER_MS) {
        pkt_per_frame = NS_PER_MS / pkt_time;
    }

    tx_ctx->st30_frame_size = pkt_per_frame * tx_ctx->pkt_len;
    ops_tx.framebuff_size = tx_ctx->st30_frame_size;
    ops_tx.framebuff_cnt = tx_ctx->framebuff_cnt;

    st30_tx_handle tx_handle = st30_tx_create(dev_handle, &ops_tx);
    if (!tx_handle) {
        printf("%s, failed to create MTL TX session.\n", __func__);
        st_pthread_mutex_destroy(&tx_ctx->st30_wake_mutex);
        st_pthread_cond_destroy(&tx_ctx->st30_wake_cond);
        free(tx_ctx->framebuffs);
        free(tx_ctx);
        return NULL;
    }
    tx_ctx->handle = tx_handle;

    /* initialize share memory */
    ret = tx_st30_shm_init(tx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        st_pthread_mutex_destroy(&tx_ctx->st30_wake_mutex);
        st_pthread_cond_destroy(&tx_ctx->st30_wake_cond);
        free(tx_ctx->framebuffs);
        free(tx_ctx);
        return NULL;
    }

    /* increase session index */
    idx++;

    return tx_ctx;
}

/* RX: Create ST30 session */
rx_st30_session_context_t* mtl_st30_rx_session_create(mtl_handle dev_handle, struct st30_rx_ops* opts, memif_ops_t* memif_ops)
{
    int ret = 0;
    static int idx = 0;
    rx_st30_session_context_t* rx_ctx = NULL;
    struct st30_rx_ops ops_rx = { 0 };

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    rx_ctx = calloc(1, sizeof(rx_st30_session_context_t));
    if (rx_ctx == NULL) {
        printf("%s, RX session contex malloc fail\n", __func__);
        return NULL;
    }

    rx_ctx->st = dev_handle;
    rx_ctx->idx = idx;
    rx_ctx->stop = false;
    st_pthread_mutex_init(&rx_ctx->st30_wake_mutex, NULL);
    st_pthread_cond_init(&rx_ctx->st30_wake_cond, NULL);

    mtl_memcpy(&ops_rx, opts, sizeof(struct st30_rx_ops));

    ops_rx.priv = rx_ctx; // app handle register to lib
    ops_rx.notify_frame_ready = rx_st30_frame_ready;
    ops_rx.notify_rtp_ready = rx_st30_rtp_ready;

    rx_ctx->pkt_len = st30_get_packet_size(opts->fmt, opts->ptime, opts->sampling, opts->channel);

    int pkt_per_frame = 1;

    double pkt_time = st30_get_packet_time(ops_rx.ptime);
    /* when ptime <= 1ms, set frame time to 1ms */
    if (pkt_time < NS_PER_MS) {
        pkt_per_frame = NS_PER_MS / pkt_time;
    }

    rx_ctx->st30_frame_size = pkt_per_frame * rx_ctx->pkt_len;
    ops_rx.framebuff_size = rx_ctx->st30_frame_size;
    rx_ctx->expect_fps = (double)NS_PER_S / st30_get_packet_time(opts->ptime) / pkt_per_frame;

    /* initialize share memory */
    ret = rx_st30_shm_init(rx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        st_pthread_mutex_destroy(&rx_ctx->st30_wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->st30_wake_cond);
        free(rx_ctx);
        return NULL;
    }

    /* Waiting for memif connection created. */
    // printf("Waiting for consumer service connection.\n");
    // while (!rx_ctx->shm_ready) {
    //     sleep(1);
    // }

    /* dump out parameters for debugging. */
    // st_rx_st30_debug_dump(ops_rx);

    st30_rx_handle rx_handle = st30_rx_create(dev_handle, &ops_rx);
    if (!rx_handle) {
        printf("%s, st30_rx_create fail\n", __func__);
        st_pthread_mutex_destroy(&rx_ctx->st30_wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->st30_wake_cond);
        free(rx_ctx);
        return NULL;
    }
    rx_ctx->handle = rx_handle;

    /* Start MTL session thread. */
    // ret = pthread_create(&rx_ctx->frame_thread, NULL, rx_st30_frame_thread, rx_ctx);
    // if (ret < 0) {
    //     printf("%s(%d), thread create fail %d\n", __func__, ret, rx_ctx->idx);
    //     st_pthread_mutex_destroy(&rx_ctx->st30_wake_mutex);
    //     st_pthread_cond_destroy(&rx_ctx->st30_wake_cond);
    //     return NULL;
    // }

    /* increase session index */
    idx++;

    return rx_ctx;
}

/* TX: Create ST40 session */
tx_st40_session_context_t* mtl_st40_tx_session_create(mtl_handle dev_handle, struct st40_tx_ops* opts, memif_ops_t* memif_ops)
{
    int ret = 0;
    tx_st40_session_context_t* tx_ctx = NULL;
    struct st40_tx_ops ops_tx = {};
    static int idx = 0;
    const int fb_cnt = 4;

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    tx_ctx = calloc(1, sizeof(tx_st40_session_context_t));
    if (tx_ctx == NULL) {
        printf("%s, TX session contex malloc fail\n", __func__);
        return NULL;
    }

    tx_ctx->framebuff_cnt = opts->framebuff_cnt;
    tx_ctx->framebuffs = (struct st_tx_frame*)calloc(1, sizeof(*tx_ctx->framebuffs) * tx_ctx->framebuff_cnt);
    if (!tx_ctx->framebuffs) {
        free(tx_ctx);
        return NULL;
    }

    for (uint16_t j = 0; j < tx_ctx->framebuff_cnt; j++) {
        tx_ctx->framebuffs[j].stat = ST_TX_FRAME_FREE;
        tx_ctx->framebuffs[j].lines_ready = 0;
    }

    tx_ctx->st = dev_handle;
    tx_ctx->idx = idx;
    tx_ctx->stop = false;
    st_pthread_mutex_init(&tx_ctx->st40_wake_mutex, NULL);
    st_pthread_cond_init(&tx_ctx->st40_wake_cond, NULL);

    mtl_memcpy(&ops_tx, opts, sizeof(struct st40_tx_ops));

    ops_tx.priv = tx_ctx; // app handle register to lib

    ops_tx.get_next_frame = tx_st40_next_frame;
    ops_tx.notify_frame_done = tx_st40_frame_done;
    ops_tx.notify_rtp_done = tx_st40_rtp_done;

    /* dump out parameters for debugging. */
    // st_tx_st40_debug_dump(ops_tx);

    st40_tx_handle tx_handle = st40_tx_create(dev_handle, &ops_tx);
    if (!tx_handle) {
        printf("%s, failed to create MTL TX session.\n", __func__);
        st_pthread_mutex_destroy(&tx_ctx->st40_wake_mutex);
        st_pthread_cond_destroy(&tx_ctx->st40_wake_cond);
        free(tx_ctx->framebuffs);
        free(tx_ctx);
        return NULL;
    }
    tx_ctx->handle = tx_handle;
    tx_ctx->pkt_len = 0xff;

    /* initialize share memory */
    ret = tx_st40_shm_init(tx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        st_pthread_mutex_destroy(&tx_ctx->st40_wake_mutex);
        st_pthread_cond_destroy(&tx_ctx->st40_wake_cond);
        free(tx_ctx->framebuffs);
        free(tx_ctx);
        return NULL;
    }

    /* increase session index */
    idx++;

    return tx_ctx;
}

/* RX: Create ST40 session */
rx_st40_session_context_t* mtl_st40_rx_session_create(mtl_handle dev_handle, struct st40_rx_ops* opts, memif_ops_t* memif_ops)
{
    int ret = 0;
    static int idx = 0;
    rx_st40_session_context_t* rx_ctx = NULL;
    struct st40_rx_ops ops_rx = { 0 };

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    rx_ctx = calloc(1, sizeof(rx_st40_session_context_t));
    if (rx_ctx == NULL) {
        printf("%s, RX session contex malloc fail\n", __func__);
        return NULL;
    }

    rx_ctx->st = dev_handle;
    rx_ctx->idx = idx;
    rx_ctx->stop = false;
    st_pthread_mutex_init(&rx_ctx->st40_wake_mutex, NULL);
    st_pthread_cond_init(&rx_ctx->st40_wake_cond, NULL);

    mtl_memcpy(&ops_rx, opts, sizeof(struct st40_rx_ops));

    ops_rx.priv = rx_ctx; // app handle register to lib
    ops_rx.notify_rtp_ready = rx_st40_rtp_ready;

    /* initialize share memory */
    ret = rx_st40_shm_init(rx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        st_pthread_mutex_destroy(&rx_ctx->st40_wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->st40_wake_cond);
        free(rx_ctx);
        return NULL;
    }

    /* Waiting for memif connection created. */
    // printf("Waiting for consumer service connection.\n");
    // while (!rx_ctx->shm_ready) {
    //     sleep(1);
    // }

    /* dump out parameters for debugging. */
    // st_rx_st40_debug_dump(ops_rx);

    st40_rx_handle rx_handle = st40_rx_create(dev_handle, &ops_rx);
    if (!rx_handle) {
        printf("%s, st40_rx_create fail\n", __func__);
        st_pthread_mutex_destroy(&rx_ctx->st40_wake_mutex);
        st_pthread_cond_destroy(&rx_ctx->st40_wake_cond);
        free(rx_ctx);
        return NULL;
    }
    rx_ctx->handle = rx_handle;
    rx_ctx->pkt_len = 0xff;

    /* Start MTL session thread. */
    ret = pthread_create(&rx_ctx->frame_thread, NULL, rx_st40_frame_thread, rx_ctx);
    if (ret < 0) {
        printf("%s(%d), thread create fail %d\n", __func__, ret, rx_ctx->idx);
        free(rx_ctx);
        return NULL;
    }

    /* increase session index */
    idx++;

    return rx_ctx;
}

/* Stop RX ST20P session */
void mtl_st20p_rx_session_stop(rx_session_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx->stop = true;

    st_pthread_mutex_lock(&rx_ctx->wake_mutex);
    st_pthread_cond_signal(&rx_ctx->wake_cond);
    st_pthread_mutex_unlock(&rx_ctx->wake_mutex);

    pthread_join(rx_ctx->frame_thread, NULL);
}

/* Stop RX ST22P session */
void mtl_st22p_rx_session_stop(rx_st22p_session_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx->stop = true;

    st_pthread_mutex_lock(&rx_ctx->st22p_wake_mutex);
    st_pthread_cond_signal(&rx_ctx->st22p_wake_cond);
    st_pthread_mutex_unlock(&rx_ctx->st22p_wake_mutex);

    pthread_join(rx_ctx->frame_thread, NULL);
}

/* Destroy RX ST20P session */
void mtl_st20p_rx_session_destroy(rx_session_context_t** p_rx_ctx)
{
    int ret = 0;
    rx_session_context_t* rx_ctx = NULL;

    if (p_rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx = *p_rx_ctx;

    printf("%s, fb_recv %d\n", __func__, rx_ctx->fb_recv);
    ret = st20p_rx_free(rx_ctx->handle);
    if (ret < 0) {
        printf("%s, session free failed\n", __func__);
        return;
    }

    st_pthread_mutex_destroy(&rx_ctx->wake_mutex);
    st_pthread_cond_destroy(&rx_ctx->wake_cond);

    rx_shm_deinit(rx_ctx);

    /* Free shared memory resource. */
    free(rx_ctx);
}

/* Destroy RX ST22P session */
void mtl_st22p_rx_session_destroy(rx_st22p_session_context_t** p_rx_ctx)
{
    int ret = 0;
    rx_st22p_session_context_t* rx_ctx = NULL;

    if (p_rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx = *p_rx_ctx;

    printf("%s, fb_recv %d\n", __func__, rx_ctx->fb_recv);
    ret = st22p_rx_free(rx_ctx->handle);
    if (ret < 0) {
        printf("%s, session free failed\n", __func__);
        return;
    }

    st_pthread_mutex_destroy(&rx_ctx->st22p_wake_mutex);
    st_pthread_cond_destroy(&rx_ctx->st22p_wake_cond);

    rx_st22p_shm_deinit(rx_ctx);

    /* Free shared memory resource. */
    free(rx_ctx);
}

/* Deinitialize IMTL */
void mtl_deinit(mtl_handle dev_handle)
{
    if (dev_handle) {
        // stop tx
        mtl_stop(dev_handle);

        mtl_uninit(dev_handle);
    }
}

/* TX: Create ST20P session */
tx_session_context_t* mtl_st20p_tx_session_create(mtl_handle dev_handle, struct st20p_tx_ops* opts, memif_ops_t* memif_ops)
{
    static int idx = 0;

    /* dst ip address for tx video session */
    const int fb_cnt = 4;

    tx_session_context_t* tx_ctx = NULL;
    int ret = 0;

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    tx_ctx = calloc(1, sizeof(tx_session_context_t));
    if (tx_ctx == NULL) {
        printf("%s, TX session contex malloc fail\n", __func__);
        return NULL;
    }

    tx_ctx->st = dev_handle;
    tx_ctx->idx = idx;
    tx_ctx->stop = false;
    st_pthread_mutex_init(&tx_ctx->wake_mutex, NULL);
    st_pthread_cond_init(&tx_ctx->wake_cond, NULL);

    struct st20p_tx_ops ops_tx = { 0 };
    if (opts == NULL) { /* set parameters to default */
        const char TX_ST20_PORT_BDF[] = "0000:31:00.0";
        static uint8_t g_tx_st20_dst_ip[MTL_IP_ADDR_LEN] = { 192, 168, 96, 2 };
        const uint16_t TX_ST20_UDP_PORT = 20000 + idx;
        const uint8_t TX_ST20_PAYLOAD_TYPE = 112;

        ops_tx.name = strdup("mcm_tx_session");
        ops_tx.port.num_port = 1;
        // tx src ip like 239.0.0.1
        memcpy(ops_tx.port.dip_addr[MTL_PORT_P], g_tx_st20_dst_ip, MTL_IP_ADDR_LEN);
        // send port interface like 0000:af:00.0
        strncpy(ops_tx.port.port[MTL_PORT_P], TX_ST20_PORT_BDF, MTL_PORT_MAX_LEN);
        ops_tx.port.payload_type = TX_ST20_PAYLOAD_TYPE;
        ops_tx.width = 1920;
        ops_tx.height = 1080;
        ops_tx.fps = ST_FPS_P60;
        ops_tx.input_fmt = ST_FRAME_FMT_YUV420CUSTOM8;
        ops_tx.transport_fmt = ST20_FMT_YUV_422_10BIT;
        ops_tx.device = ST_PLUGIN_DEVICE_AUTO;
        ops_tx.port.udp_port[MTL_PORT_P] = TX_ST20_UDP_PORT;
        ops_tx.framebuff_cnt = fb_cnt;
    } else {
        mtl_memcpy(&ops_tx, opts, sizeof(struct st20p_tx_ops));
    }

    ops_tx.priv = tx_ctx; // app handle register to lib
    ops_tx.notify_frame_available = tx_st20p_frame_available;
    ops_tx.notify_frame_done = tx_st20p_frame_done;

#if defined(ZERO_COPY)
    ops_tx.flags |= ST20P_TX_FLAG_EXT_FRAME;
#endif

    /* dump out parameters for debugging. */
    st_tx_debug_dump(ops_tx);

    st20p_tx_handle tx_handle = st20p_tx_create(dev_handle, &ops_tx);
    if (!tx_handle) {
        printf("%s, failed to create MTL TX session.\n", __func__);
        st_pthread_mutex_destroy(&tx_ctx->wake_mutex);
        st_pthread_cond_destroy(&tx_ctx->wake_cond);
        return NULL;
    }
    tx_ctx->handle = tx_handle;
    tx_ctx->frame_size = st20_frame_size(ops_tx.transport_fmt, ops_tx.width, ops_tx.height);

    /* initialize share memory */
    ret = tx_st20p_shm_init(tx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        st_pthread_mutex_destroy(&tx_ctx->wake_mutex);
        st_pthread_cond_destroy(&tx_ctx->wake_cond);
        return NULL;
    }

    /* increase session index */
    idx++;

    return tx_ctx;
}

/* TX: Create ST22P session */
tx_st22p_session_context_t* mtl_st22p_tx_session_create(mtl_handle dev_handle, struct st22p_tx_ops* opts, memif_ops_t* memif_ops)
{
    struct st22p_tx_ops ops_tx = { 0 };
    static int idx = 0;

    /* dst ip address for tx video session */
    const int fb_cnt = 4;

    tx_st22p_session_context_t* tx_ctx = NULL;
    int ret = 0;

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    tx_ctx = calloc(1, sizeof(tx_st22p_session_context_t));
    if (tx_ctx == NULL) {
        printf("%s, TX session contex malloc fail\n", __func__);
        return NULL;
    }

    tx_ctx->st = dev_handle;
    tx_ctx->idx = idx;
    tx_ctx->stop = false;
    tx_ctx->fb_cnt = 3;
    tx_ctx->fb_idx = 0;
    st_pthread_mutex_init(&tx_ctx->st22p_wake_mutex, NULL);
    st_pthread_cond_init(&tx_ctx->st22p_wake_cond, NULL);

    mtl_memcpy(&ops_tx, opts, sizeof(struct st22p_tx_ops));

    ops_tx.priv = tx_ctx; // app handle register to lib
    ops_tx.notify_frame_available = tx_st22p_frame_available;
    ops_tx.notify_frame_done = tx_st22p_frame_done;

#if defined(ZERO_COPY)
    ops_tx.flags |= ST22P_TX_FLAG_EXT_FRAME;
#endif

    /* dump out parameters for debugging. */
    st_tx_st22p_debug_dump(ops_tx);

    st22p_tx_handle tx_handle = st22p_tx_create(dev_handle, &ops_tx);
    if (!tx_handle) {
        printf("%s, failed to create MTL TX session.\n", __func__);
        st_pthread_mutex_destroy(&tx_ctx->st22p_wake_mutex);
        st_pthread_cond_destroy(&tx_ctx->st22p_wake_cond);
        return NULL;
    }

    tx_ctx->handle = tx_handle;
    tx_ctx->frame_size = st22p_tx_frame_size(tx_handle);

    /* initialize share memory */
    ret = tx_st22p_shm_init(tx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        st_pthread_mutex_destroy(&tx_ctx->st22p_wake_mutex);
        st_pthread_cond_destroy(&tx_ctx->st22p_wake_cond);
        return NULL;
    }

    /* increase session index */
    idx++;

    return tx_ctx;
}

/* TX: Destroy ST20P session */
void mtl_st20p_tx_session_destroy(tx_session_context_t** p_tx_ctx)
{
    tx_session_context_t* tx_ctx = NULL;

    if (p_tx_ctx == NULL) {
        printf("%s:%d Invalid parameter\n", __func__, __LINE__);
        return;
    }

    tx_ctx = *p_tx_ctx;
    if (tx_ctx == NULL || tx_ctx->handle == NULL) {
        printf("%s:%d Invalid parameter\n", __func__, __LINE__);
        return;
    }

    printf("%s, fb_send %d\n", __func__, tx_ctx->fb_send);
    if (st20p_tx_free(tx_ctx->handle) < 0) {
        printf("%s, session free failed\n", __func__);
        return;
    }

    st_pthread_mutex_destroy(&tx_ctx->wake_mutex);
    st_pthread_cond_destroy(&tx_ctx->wake_cond);

    /* Free shared memory resource. */
    tx_shm_deinit(tx_ctx);

    /* FIXME: should be freed. */
    free(tx_ctx);
}

/* TX: Destroy ST22P session */
void mtl_st22p_tx_session_destroy(tx_st22p_session_context_t** p_tx_ctx)
{
    tx_st22p_session_context_t* tx_ctx = NULL;

    if (p_tx_ctx == NULL) {
        printf("%s:%d Invalid parameter\n", __func__, __LINE__);
        return;
    }

    tx_ctx = *p_tx_ctx;
    if (tx_ctx == NULL || tx_ctx->handle == NULL) {
        printf("%s:%d Invalid parameter\n", __func__, __LINE__);
        return;
    }

    printf("%s, fb_send %d\n", __func__, tx_ctx->fb_send);
    if (st22p_tx_free(tx_ctx->handle) < 0) {
        printf("%s, session free failed\n", __func__);
        return;
    }

    st_pthread_mutex_destroy(&tx_ctx->st22p_wake_mutex);
    st_pthread_cond_destroy(&tx_ctx->st22p_wake_cond);

    /* Free shared memory resource. */
    tx_st22p_shm_deinit(tx_ctx);

    /* FIXME: should be freed. */
    free(tx_ctx);
}

/* TX: Stop ST20P session */
void mtl_st20p_tx_session_stop(tx_session_context_t* tx_ctx)
{
    if (tx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    if (!tx_ctx->shm_ready) {
        pthread_cancel(tx_ctx->memif_event_thread);
    }

    tx_ctx->stop = true;

    st_pthread_mutex_lock(&tx_ctx->wake_mutex);
    st_pthread_cond_signal(&tx_ctx->wake_cond);
    st_pthread_mutex_unlock(&tx_ctx->wake_mutex);

    pthread_join(tx_ctx->memif_event_thread, NULL);
}

/* TX: Stop ST22P session */
void mtl_st22p_tx_session_stop(tx_st22p_session_context_t* tx_ctx)
{
    if (tx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    if (!tx_ctx->shm_ready) {
        pthread_cancel(tx_ctx->memif_event_thread);
    }

    tx_ctx->stop = true;

    st_pthread_mutex_lock(&tx_ctx->st22p_wake_mutex);
    st_pthread_cond_signal(&tx_ctx->st22p_wake_cond);
    st_pthread_mutex_unlock(&tx_ctx->st22p_wake_mutex);

    pthread_join(tx_ctx->memif_event_thread, NULL);
}

/* TX: Stop ST30 session */
void mtl_st30_tx_session_stop(tx_st30_session_context_t* pctx)
{
    if (pctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    if (!pctx->shm_ready) {
        pthread_cancel(pctx->memif_event_thread);
    }

    pctx->stop = true;

    st_pthread_mutex_lock(&pctx->st30_wake_mutex);
    st_pthread_cond_signal(&pctx->st30_wake_cond);
    st_pthread_mutex_unlock(&pctx->st30_wake_mutex);

    pthread_join(pctx->memif_event_thread, NULL);
}

/* RX: Stop ST30 session */
void mtl_st30_rx_session_stop(rx_st30_session_context_t* pctx)
{
    if (pctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    pctx->stop = true;

    st_pthread_mutex_lock(&pctx->st30_wake_mutex);
    st_pthread_cond_signal(&pctx->st30_wake_cond);
    st_pthread_mutex_unlock(&pctx->st30_wake_mutex);

    pthread_join(pctx->frame_thread, NULL);
}

/* TX: Destroy ST30 session */
void mtl_st30_tx_session_destroy(tx_st30_session_context_t** ppctx)
{
    tx_st30_session_context_t* pctx = NULL;

    if (ppctx == NULL) {
        printf("%s:%d Invalid parameter\n", __func__, __LINE__);
        return;
    }

    pctx = *ppctx;
    if (pctx == NULL || pctx->handle == NULL) {
        printf("%s:%d Invalid parameter\n", __func__, __LINE__);
        return;
    }

    printf("%s, fb_send %d\n", __func__, pctx->fb_send);
    if (st30_tx_free(pctx->handle) < 0) {
        printf("%s, session free failed\n", __func__);
        return;
    }

    st_pthread_mutex_destroy(&pctx->st30_wake_mutex);
    st_pthread_cond_destroy(&pctx->st30_wake_cond);

    /* Free shared memory resource. */
    tx_st30_shm_deinit(pctx);

    /* FIXME: should be freed. */
    free(pctx);
}

/* RX: Destroy ST30 session */
void mtl_st30_rx_session_destroy(rx_st30_session_context_t** ppctx)
{
    int ret = 0;
    rx_st30_session_context_t* pctx = NULL;

    if (ppctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    pctx = *ppctx;

    printf("%s, fb_recv %d\n", __func__, pctx->fb_recv);
    ret = st30_rx_free(pctx->handle);
    if (ret < 0) {
        printf("%s, session free failed\n", __func__);
        return;
    }

    st_pthread_mutex_destroy(&pctx->st30_wake_mutex);
    st_pthread_cond_destroy(&pctx->st30_wake_cond);

    rx_st30_shm_deinit(pctx);

    /* Free shared memory resource. */
    free(pctx);
}

/* TX: Stop ST40 session */
void mtl_st40_tx_session_stop(tx_st40_session_context_t* pctx)
{
    if (pctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    if (!pctx->shm_ready) {
        pthread_cancel(pctx->memif_event_thread);
    }

    pctx->stop = true;

    st_pthread_mutex_lock(&pctx->st40_wake_mutex);
    st_pthread_cond_signal(&pctx->st40_wake_cond);
    st_pthread_mutex_unlock(&pctx->st40_wake_mutex);

    pthread_join(pctx->memif_event_thread, NULL);
}

/* RX: Stop ST40 session */
void mtl_st40_rx_session_stop(rx_st40_session_context_t* pctx)
{
    if (pctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    pctx->stop = true;

    st_pthread_mutex_lock(&pctx->st40_wake_mutex);
    st_pthread_cond_signal(&pctx->st40_wake_cond);
    st_pthread_mutex_unlock(&pctx->st40_wake_mutex);

    pthread_join(pctx->frame_thread, NULL);
}

/* TX: Destroy ST40 session */
void mtl_st40_tx_session_destroy(tx_st40_session_context_t** ppctx)
{
    tx_st40_session_context_t* pctx = NULL;

    if (ppctx == NULL) {
        printf("%s:%d Invalid parameter\n", __func__, __LINE__);
        return;
    }

    pctx = *ppctx;
    if (pctx == NULL || pctx->handle == NULL) {
        printf("%s:%d Invalid parameter\n", __func__, __LINE__);
        return;
    }

    printf("%s, fb_send %d\n", __func__, pctx->fb_send);
    if (st40_tx_free(pctx->handle) < 0) {
        printf("%s, session free failed\n", __func__);
        return;
    }

    st_pthread_mutex_destroy(&pctx->st40_wake_mutex);
    st_pthread_cond_destroy(&pctx->st40_wake_cond);

    /* Free shared memory resource. */
    tx_st40_shm_deinit(pctx);

    /* FIXME: should be freed. */
    free(pctx);
}

/* RX: Destroy ST40 session */
void mtl_st40_rx_session_destroy(rx_st40_session_context_t** ppctx)
{
    int ret = 0;
    rx_st40_session_context_t* pctx = NULL;

    if (ppctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    pctx = *ppctx;

    printf("%s, fb_recv %d\n", __func__, pctx->fb_recv);
    ret = st40_rx_free(pctx->handle);
    if (ret < 0) {
        printf("%s, session free failed\n", __func__);
        return;
    }

    st_pthread_mutex_destroy(&pctx->st40_wake_mutex);
    st_pthread_cond_destroy(&pctx->st40_wake_cond);

    rx_st40_shm_deinit(pctx);

    /* Free shared memory resource. */
    free(pctx);
}

int rx_udp_h264_shm_deinit(rx_udp_h264_session_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(rx_ctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&rx_ctx->memif_conn);
    memif_delete_socket(&rx_ctx->memif_socket);

    /* unlink socket file */
    if (rx_ctx->memif_conn_args.is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        unlink(rx_ctx->memif_socket_args.path);
    }

    return 0;
}

/* Stop RX UDP H264 session */
void mtl_rtsp_rx_session_stop(rx_udp_h264_session_context_t* rx_ctx)
{
    int ret;
    if (rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx->stop = true;

    ret = mtl_sch_unregister_tasklet(rx_ctx->udp_tasklet);
    if (ret != 0) {
        INFO("%s, mtl_sch_unregister_tasklet fail %d\n", __func__, ret);
    }

    ret = mudp_close(rx_ctx->socket);
    if (ret < 0) {
        INFO("%s, udp close fail %d\n", __func__, ret);
        // return NULL;
    }
}

/* Destroy RX UDP h264 session */
void mtl_rtsp_rx_session_destroy(rx_udp_h264_session_context_t** p_rx_ctx)
{
    int ret = 0;
    rx_udp_h264_session_context_t* rx_ctx = NULL;

    if (p_rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx = *p_rx_ctx;

    rx_udp_h264_shm_deinit(rx_ctx);

    /* Free shared memory resource. */
    free(rx_ctx);
}
