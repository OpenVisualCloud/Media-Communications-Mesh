/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "session-mtl.h"

void TxSt30MtlSession::copy_connection_params(const mcm_conn_param &request, std::string &dev_port)
{
    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st30_%d", get_id());

    inet_pton(AF_INET, request.remote_addr.ip, ops.port.dip_addr[MTL_PORT_P]);
    ops.port.udp_port[MTL_PORT_P] = atoi(request.remote_addr.port);
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
    printf("dip_addr    :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %u", ops.port.dip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops.port.num_port);
    INFO("udp_port      : %d", ops.port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops.port.payload_type);
    INFO("name          : %s", ops.name);
    INFO("framebuff_cnt : %d", ops.framebuff_cnt);
}

TxSt30MtlSession::TxSt30MtlSession(mtl_handle dev_handle, const mcm_conn_param &request,
                                   std::string dev_port, memif_ops_t &memif_ops)
    : MtlSession(memif_ops, request.payload_type, TX, dev_handle), handle(0), fb_send(0), ops{0}
{
    copy_connection_params(request, dev_port);

    ops.priv = this; // app handle register to lib
    ops.notify_frame_available = frame_available_callback_wrapper;
    ops.framebuff_size = st30_get_packet_size(ops.fmt, ops.ptime, ops.sampling, ops.channel);
}

int TxSt30MtlSession::init()
{
    handle = st30p_tx_create(st, &ops);
    if (!handle) {
        ERROR("Failed to create MTL TX ST30 session.");
        return -1;
    }

    int ret = shm_init(ops.framebuff_size, 4);
    if (ret < 0) {
        ERROR("Failed to initialize shared memory");
        return -1;
    }
    return 0;
}

TxSt30MtlSession::~TxSt30MtlSession()
{
    INFO("%s, fb_send %d\n", __func__, fb_send);
    stop = true;
    if (handle) {
        st30p_tx_free(handle);
        handle = 0;
    }
}

int TxSt30MtlSession::on_receive_cb(memif_conn_handle_t conn, uint16_t qid)
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

    struct st30_frame *frame = NULL;
    do {
        frame = st30p_tx_get_frame(handle);
        if (!frame) {
            pthread_mutex_lock(&wake_mutex);
            if (!stop)
                pthread_cond_wait(&wake_cond, &wake_mutex);
            pthread_mutex_unlock(&wake_mutex);
        }
    } while (!frame);

    /* fill frame data. */
    mtl_memcpy(frame->addr, shm_bufs.data, shm_bufs.len);
    /* Send out frame. */
    st30p_tx_put_frame(handle, frame);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    return 0;
}
