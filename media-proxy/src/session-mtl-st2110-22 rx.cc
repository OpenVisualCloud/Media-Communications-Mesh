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
    if (!priv) {
        return -1;
    }
    RxSt22MtlSession *s = (RxSt22MtlSession *)priv;
    return s->query_ext_frame_cb(ext_frame, meta);
}

int RxSt22MtlSession::query_ext_frame_cb(struct st_ext_frame *ext_frame,
                                         struct st22_rx_frame_meta *meta)
{
    /* allocate memory */
    uint16_t qid = 0;
    uint16_t rx_buf_num = 0;

    if (!atomic_load_explicit(&shm_ready, std::memory_order_relaxed)) {
        ERROR("rx_st22p_query_ext_frame: MemIF connection not ready.");
        return -1;
    }

    memif_buffer_t shm_buf = {0};
    int err = memif_buffer_alloc(memif_conn, qid, &shm_buf, 1, &rx_buf_num, frame_size);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st22p_query_ext_frame: Failed to alloc memif buffer: %s", memif_strerror(err));
        return -1;
    }

    uint8_t planes = st_frame_fmt_planes(ops.output_fmt);
    mtl_iova_t ext_fb_iova = source_begin_iova + ((uint8_t *)shm_buf.data - source_begin);

    for (uint8_t plane = 0; plane < planes; plane++) { /* assume planes continuous */
        ext_frame->linesize[plane] = st_frame_least_linesize(ops.output_fmt, ops.width, plane);
        if (plane == 0) {
            ext_frame->addr[plane] = shm_buf.data;
            ext_frame->iova[plane] = ext_fb_iova;
        } else {
            ext_frame->addr[plane] =
                (uint8_t *)ext_frame->addr[plane - 1] + ext_frame->linesize[plane - 1] * ops.height;
            ext_frame->iova[plane] =
                ext_frame->iova[plane - 1] + ext_frame->linesize[plane - 1] * ops.height;
        }
    }
    ext_frame->size = frame_size;

    fifo_mtx.lock();
    fifo.push(shm_buf);
    fifo_mtx.unlock();

    return 0;
}

int RxSt22MtlSession::on_connect_cb(memif_conn_handle_t conn)
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

    return Session::on_connect_cb(conn);
}

int RxSt22MtlSession::on_disconnect_cb(memif_conn_handle_t conn)
{
    if (atomic_load_explicit(&shm_ready, std::memory_order_relaxed) &&
        mtl_dma_unmap(st, source_begin, source_begin_iova, source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }

    return Session::on_disconnect_cb(conn);
}
#endif

void RxSt22MtlSession::frame_thread()
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

void RxSt22MtlSession::copy_connection_params(const mcm_conn_param &request, std::string dev_port)
{
    char session_name[NAME_MAX] = "";
    snprintf(session_name, NAME_MAX, "mcm_rx_st22_%d", get_id());

    inet_pton(AF_INET, request.remote_addr.ip, ops.port.ip_addr[MTL_PORT_P]);
    inet_pton(AF_INET, request.local_addr.ip, ops.port.mcast_sip_addr[MTL_PORT_P]);
    ops.port.udp_port[MTL_PORT_P] = atoi(request.local_addr.port);

    // ops->port.udp_port[MTL_PORT_P] = RX_ST20_UDP_PORT;
    strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    ops.port.num_port = 1;
    if (request.payload_type_nr == 0) {
        ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST22;
    } else {
        ops.port.payload_type = request.payload_type_nr;
    }
    ops.name = strdup(session_name);
    ops.width = request.width;
    ops.height = request.height;
    ops.fps = st_frame_rate_to_st_fps((double)request.fps);
    ops.output_fmt = get_st_frame_fmt(request.pix_fmt);
    ops.device = ST_PLUGIN_DEVICE_AUTO;
    ops.framebuff_cnt = 4;
    ops.pack_type = ST22_PACK_CODESTREAM;
    ops.codec = ST22_CODEC_JPEGXS;
    ops.codec_thread_cnt = 0;
    ops.max_codestream_size = 0;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops.port.port[MTL_PORT_P]);
    printf("INFO: ip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops.port.ip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    printf("INFO: mcast_sip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops.port.mcast_sip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops.port.num_port);
    INFO("udp_port      : %d", ops.port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops.port.payload_type);
    INFO("name          : %s", ops.name);
    INFO("width         : %d", ops.width);
    INFO("height        : %d", ops.height);
    INFO("fps           : %d", ops.fps);
    INFO("output_fmt    : %d", ops.output_fmt);
    INFO("device        : %d", ops.device);
    INFO("framebuff_cnt : %d", ops.framebuff_cnt);
}

RxSt22MtlSession::RxSt22MtlSession(mtl_handle dev_handle, const mcm_conn_param &request,
                                   std::string dev_port, memif_ops_t &memif_ops)
    : MtlSession(memif_ops, request.payload_type, RX, dev_handle), handle(0),
      frame_thread_handle(0), fb_recv(0), ops{0}
{
    copy_connection_params(request, dev_port);

    frame_size = st_frame_size(ops.output_fmt, ops.width, ops.height, false);

    ops.priv = this; // app handle register to lib
    ops.notify_frame_available = frame_available_callback_wrapper;

#if defined(MTL_ZERO_COPY)
    ops.flags |= ST22P_RX_FLAG_EXT_FRAME;
    ops.flags |= ST22P_RX_FLAG_RECEIVE_INCOMPLETE_FRAME;
    ops.query_ext_frame = query_ext_frame_callback_wrapper;
#endif
}

int RxSt22MtlSession::init()
{
    int ret = shm_init(frame_size, 2);
    if (ret < 0) {
        ERROR("Failed to initialize shared memory");
        return -1;
    }

    handle = st22p_rx_create(st, &ops);
    if (!handle) {
        ERROR("Failed to create MTL RX ST22 session");
        return -1;
    }

    if (frame_size != st22p_rx_frame_size(handle)) {
        ERROR("Frame size differ!!!");
        return -1;
    }

    /* Start MTL session thread. */
    frame_thread_handle = new std::thread(&RxSt22MtlSession::frame_thread, this);
    if (!frame_thread_handle) {
        ERROR("Failed to create thread");
        return -1;
    }
    return 0;
}

RxSt22MtlSession::~RxSt22MtlSession()
{
    INFO("%s, fb_recv %d\n", __func__, fb_recv);
    stop = true;
    if (frame_thread_handle) {
        frame_thread_handle->join();
        delete frame_thread_handle;
        frame_thread_handle = 0;
    }

    if (handle) {
        st22p_rx_free(handle);
        handle = 0;
    }
}

void RxSt22MtlSession::consume_frame(struct st_frame *frame)
{
    int err = 0;
    uint16_t qid = 0;
    memif_buffer_t rx_buf = {0};
    uint16_t rx_buf_num = 0, rx = 0;

    if (!atomic_load_explicit(&shm_ready, std::memory_order_relaxed)) {
        INFO("%s memif not ready\n", __func__);
        return;
    }

#if defined(MTL_ZERO_COPY)
    if (fifo.empty()) {
        ERROR("%s FIFO empty \n", __func__);
        return;
    }

    fifo_mtx.lock();
    rx_buf = fifo.front();
    fifo.pop();
    fifo_mtx.unlock();
    rx_buf_num = 1;
#else
    err = memif_buffer_alloc_timeout(memif_conn, qid, &rx_buf, 1, &rx_buf_num, frame_size, 10);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st22p consume_frame Failed to alloc memif buffer: %s", memif_strerror(err));
        return;
    }

    mtl_memcpy(rx_buf.data, frame->addr[0], frame_size);
#endif

    /* Send to microservice application. */
    err = memif_tx_burst(memif_conn, qid, &rx_buf, rx_buf_num, &rx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st22p consume_frame memif_tx_burst: %s", memif_strerror(err));
    }

    fb_recv++;
}
