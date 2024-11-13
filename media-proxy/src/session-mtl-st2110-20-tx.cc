/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "session-mtl.h"

#if defined(MTL_ZERO_COPY)
static int tx_frame_done_callback_wrapper(void *priv, struct st_frame *frame)
{
    if (!priv) {
        return -1;
    }
    TxSt20MtlSession *s = (TxSt20MtlSession *)priv;
    return s->frame_done_cb(frame);
}

int TxSt20MtlSession::frame_done_cb(struct st_frame *frame)
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

int TxSt20MtlSession::on_connect_cb(memif_conn_handle_t conn)
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

int TxSt20MtlSession::on_disconnect_cb(memif_conn_handle_t conn)
{
    if (atomic_load_explicit(&shm_ready, std::memory_order_relaxed) &&
        mtl_dma_unmap(st, source_begin, source_begin_iova, source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }

    return Session::on_disconnect_cb(conn);
}
#endif

void TxSt20MtlSession::copy_connection_params(const mcm_conn_param &request, std::string dev_port)
{
    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st20_%d", get_id());

    inet_pton(AF_INET, request.remote_addr.ip, ops.port.dip_addr[MTL_PORT_P]);
    ops.port.udp_port[MTL_PORT_P] = atoi(request.remote_addr.port);
    strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    ops.port.udp_src_port[MTL_PORT_P] = atoi(request.local_addr.port);
    ops.port.num_port = 1;
    if (request.payload_type_nr == 0) {
        ops.port.payload_type = ST_APP_PAYLOAD_TYPE_VIDEO;
    } else {
        ops.port.payload_type = request.payload_type_nr;
    }
    ops.name = strdup(session_name);
    ops.width = request.width;
    ops.height = request.height;
    ops.fps = st_frame_rate_to_st_fps((double)request.fps);
    ops.input_fmt = get_st_frame_fmt(request.pix_fmt);
    ops.transport_fmt = ST20_FMT_YUV_422_PLANAR10LE;
    ops.device = ST_PLUGIN_DEVICE_AUTO;
    ops.framebuff_cnt = 4;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops.port.port[MTL_PORT_P]);
    printf("dip_addr    :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %u", ops.port.dip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops.port.num_port);
    INFO("udp_port      : %d", ops.port.udp_port[MTL_PORT_P]);
    INFO("udp_src_port  : %d", ops.port.udp_src_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops.port.payload_type);
    INFO("name          : %s", ops.name);
    INFO("width         : %d", ops.width);
    INFO("height        : %d", ops.height);
    INFO("fps           : %d", ops.fps);
    INFO("transport_fmt : %d", ops.transport_fmt);
    INFO("input_fmt     : %d", ops.input_fmt);
    INFO("device        : %d", ops.device);
    INFO("framebuff_cnt : %d", ops.framebuff_cnt);
}

TxSt20MtlSession::TxSt20MtlSession(mtl_handle dev_handle, const mcm_conn_param &request,
                                   std::string dev_port, memif_ops_t &memif_ops)
    : MtlSession(memif_ops, request.payload_type, TX, dev_handle), handle(0), fb_send(0), ops{0}
{
    copy_connection_params(request, dev_port);

    frame_size = st_frame_size(ops.input_fmt, ops.width, ops.height, false);

    ops.priv = this; // app handle register to lib
    ops.notify_frame_available = frame_available_callback_wrapper;

#if defined(MTL_ZERO_COPY)
    ops.notify_frame_done = tx_frame_done_callback_wrapper;
    ops.flags |= ST20P_TX_FLAG_EXT_FRAME;
    source_begin_iova_map_sz = 0;
#endif
}

int TxSt20MtlSession::init()
{
    handle = st20p_tx_create(st, &ops);
    if (!handle) {
        ERROR("Failed to create MTL TX ST20 session");
        return -1;
    }

    int ret = shm_init(frame_size, 2);
    if (ret < 0) {
        ERROR("Failed to initialize shared memory");
        return -1;
    }
    return 0;
}

TxSt20MtlSession::~TxSt20MtlSession()
{
    INFO("%s, fb_send %d\n", __func__, fb_send);
    stop = true;
    if (handle) {
        st20p_tx_free(handle);
        handle = 0;
    }
}

int TxSt20MtlSession::on_receive_cb(memif_conn_handle_t conn, uint16_t qid)
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

    struct st_frame *frame = NULL;
    do {
        frame = st20p_tx_get_frame(handle);
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

    st20p_tx_put_ext_frame(handle, frame, &ext_frame);
#else
    /* fill frame data. */
    mtl_memcpy(frame->addr[0], shm_bufs.data, shm_bufs.len);
    /* Send out frame. */
    st20p_tx_put_frame(handle, frame);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));
#endif

    fb_send++;

    return 0;
}
