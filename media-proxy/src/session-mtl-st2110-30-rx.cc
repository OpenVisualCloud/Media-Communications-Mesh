/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "session-mtl.h"

void RxSt30MtlSession::frame_thread()
{
    INFO("%s, start\n", __func__);
    while (!stop) {
        struct st30_frame *frame = st30p_rx_get_frame(handle);
        if (!frame) { /* no frame */
            pthread_mutex_lock(&wake_mutex);
            if (!stop)
                pthread_cond_wait(&wake_cond, &wake_mutex);
            pthread_mutex_unlock(&wake_mutex);
            continue;
        }
        consume_frame(frame);
        st30p_rx_put_frame(handle, frame);
    }
}

void RxSt30MtlSession::copy_connection_params(const mcm_conn_param &request, std::string dev_port)
{
    char session_name[NAME_MAX] = "";
    snprintf(session_name, NAME_MAX, "mcm_rx_st30_%d", get_id());

    inet_pton(AF_INET, request.remote_addr.ip, ops.port.ip_addr[MTL_PORT_P]);
    ops.port.udp_port[MTL_PORT_P] = atoi(request.local_addr.port);

    strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    ops.port.num_port = 1;
    ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST30;
    ops.name = strdup(session_name);
    ops.framebuff_cnt = 4;

    ops.fmt = (st30_fmt)request.payload_args.audio_args.format;
    ops.channel = request.payload_args.audio_args.channel;
    ops.sampling = (st30_sampling)request.payload_args.audio_args.sampling;
    ops.ptime = (st30_ptime)request.payload_args.audio_args.ptime;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops.port.port[MTL_PORT_P]);
    printf("INFO: ip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops.port.ip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops.port.num_port);
    INFO("udp_port      : %d", ops.port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops.port.payload_type);
    INFO("name          : %s", ops.name);
    INFO("framebuff_cnt : %d", ops.framebuff_cnt);
}

RxSt30MtlSession::RxSt30MtlSession(mtl_handle dev_handle, const mcm_conn_param &request,
                                   std::string dev_port, memif_ops_t &memif_ops)
    : MtlSession(memif_ops, request.payload_type, RX, dev_handle), handle(0), fb_recv(0), ops{0},
      frame_thread_handle(0)
{
    copy_connection_params(request, dev_port);

    ops.priv = this; // app handle register to lib
    ops.notify_frame_available = frame_available_callback_wrapper;
    ops.framebuff_size = st30_get_packet_size(ops.fmt, ops.ptime, ops.sampling, ops.channel);
}

int RxSt30MtlSession::init()
{
    int ret = shm_init(ops.framebuff_size, 4);
    if (ret < 0) {
        ERROR("Failed to initialize shared memory");
        return -1;
    }

    handle = st30p_rx_create(st, &ops);
    if (!handle) {
        ERROR("Failed to initialize MTL RX ST30 session\n");
        return -1;
    }

    /* Start MTL session thread. */
    frame_thread_handle = new std::thread(&RxSt30MtlSession::frame_thread, this);
    if (!frame_thread_handle) {
        ERROR("Failed to create thread");
        return -1;
    }
    return 0;
}

RxSt30MtlSession::~RxSt30MtlSession()
{
    INFO("%s, fb_recv %d\n", __func__, fb_recv);
    stop = true;
    if (frame_thread_handle) {
        frame_thread_handle->join();
        delete frame_thread_handle;
        frame_thread_handle = 0;
    }

    if (handle) {
        st30p_rx_free(handle);
        handle = 0;
    }
}

void RxSt30MtlSession::consume_frame(struct st30_frame *frame)
{
    int err = 0;
    uint16_t qid = 0;
    memif_buffer_t rx_buf;
    uint16_t rx_buf_num = 0, rx = 0;

    if (!atomic_load_explicit(&shm_ready, std::memory_order_relaxed)) {
        INFO("%s memif not ready\n", __func__);
        return;
    }

    err = memif_buffer_alloc_timeout(memif_conn, qid, &rx_buf, 1, &rx_buf_num, ops.framebuff_size,
                                     10);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st30p consume_frame: Failed to alloc memif buffer: %s", memif_strerror(err));
        return;
    }

    mtl_memcpy(rx_buf.data, frame->addr, ops.framebuff_size);

    /* Send to microservice application. */
    err = memif_tx_burst(memif_conn, qid, &rx_buf, rx_buf_num, &rx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_st20p consume_frame memif_tx_burst: %s", memif_strerror(err));
    }

    fb_recv++;
}
