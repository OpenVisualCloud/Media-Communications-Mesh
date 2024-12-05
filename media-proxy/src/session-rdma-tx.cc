/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pthread.h>
#include <sys/stat.h>
#include <bsd/string.h>

#include "libfabric_dev.h"
#include "session-rdma.h"

void TxRdmaSession::handle_sent_buffers()
{
    int err = libfabric_ep_ops.ep_cq_read(ep_ctx, NULL, 1);
    if (err) {
        if (err != -EAGAIN)
            INFO("%s ep_txcq_read: %s", __func__, strerror(-err));
        return;
    }
    fb_send++;

    err = memif_refill_queue(memif_conn, 0, 1, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));
}

void TxRdmaSession::frame_thread()
{
    while (!atomic_load_explicit(&shm_ready, std::memory_order_acquire) && !stop)
        usleep(1000);

    INFO("%s, TX RDMA thread started\n", __func__);
    while (!stop) {
        if (!atomic_load_explicit(&shm_ready, std::memory_order_acquire))
            continue;
        handle_sent_buffers();
    }
}

TxRdmaSession::TxRdmaSession(libfabric_ctx *dev_handle, const mcm_conn_param &request,
                             memif_ops_t &memif_ops)
    : Session(memif_ops, request.payload_type, TX), ep_cfg{0}, ep_ctx(0), stop(false),
      frame_thread_handle(0), fb_send(0)
{
    transfer_size = request.payload_args.rdma_args.transfer_size;

    ep_cfg.rdma_ctx = dev_handle;
    memcpy(&ep_cfg.remote_addr, &request.remote_addr, sizeof(request.remote_addr));
    memcpy(&ep_cfg.local_addr, &request.local_addr, sizeof(request.local_addr));
}

int TxRdmaSession::init()
{
    int err = libfabric_ep_ops.ep_init(&ep_ctx, &ep_cfg);
    if (err) {
        ERROR("Failed to initialize libfabric's end point");
        return -1;
    }

    err = shm_init(transfer_size, 4);
    if (err < 0) {
        ERROR("Failed to to initialize shared memory");
        return -1;
    }

    frame_thread_handle = new std::thread(&TxRdmaSession::frame_thread, this);
    if (!frame_thread_handle) {
        ERROR("Failed to create thread");
        return -1;
    }
    return 0;
}

TxRdmaSession::~TxRdmaSession()
{
    INFO("%s, fb_send %d\n", __func__, fb_send);
    stop = true;
    if (ep_ctx) {
        if (libfabric_ep_ops.ep_destroy(&ep_ctx)) {
            ERROR("Failed to destroy RDMA context");
        }
        ep_ctx = 0;
    }

    if (frame_thread_handle) {
        frame_thread_handle->join();
        delete frame_thread_handle;
        frame_thread_handle = 0;
    }
}

int TxRdmaSession::on_receive_cb(memif_conn_handle_t conn, uint16_t qid)
{
    memif_buffer_t shm_bufs = {0};
    uint16_t buf_num = 0;
    int err = 0;

    if (stop) {
        INFO("TX session already stopped.");
        return -EINVAL;
    }

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    err = libfabric_ep_ops.ep_send_buf(ep_ctx, shm_bufs.data, shm_bufs.len);
    if (err) {
        ERROR("ep_send_buf failed with: %s", fi_strerror(-err));
        return err;
    }

    return 0;
}

int TxRdmaSession::on_connect_cb(memif_conn_handle_t conn)
{
    memif_region_details_t region;

    int err = memif_get_buffs_region(conn, &region);
    if (err) {
        ERROR("%s, Getting memory buffers from memif failed. \n", __func__);
        return err;
    }

    err = libfabric_ep_ops.ep_reg_mr(ep_ctx, region.addr, region.size);
    if (err) {
        ERROR("%s, ep_reg_mr failed: %s\n", __func__, fi_strerror(-err));
        return err;
    }

    return Session::on_connect_cb(conn);
}

int TxRdmaSession::on_disconnect_cb(memif_conn_handle_t conn)
{
    /* TODO: unregister in libfabric memory regions allocated by memif */

    return Session::on_disconnect_cb(conn);
}
