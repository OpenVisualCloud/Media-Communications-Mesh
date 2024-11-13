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

shm_buf_info_t *RxRdmaSession::get_free_shm_buf()
{
    for (uint32_t i = 0; i < shm_buf_num; i++) {
        if (!shm_bufs[i].used) {
            return &shm_bufs[i];
        }
    }
    return NULL;
}

int RxRdmaSession::pass_empty_buf_to_libfabric()
{
    int err;
    shm_buf_info_t *buf_info = NULL;
    uint16_t rx_buf_num = 0;

    buf_info = get_free_shm_buf();
    if (!buf_info)
        return -ENOMEM;

    err = memif_buffer_alloc(memif_conn, 0, &buf_info->shm_buf, 1, &rx_buf_num, transfer_size);
    if (err != MEMIF_ERR_SUCCESS)
        return -ENOMEM;

    buf_info->used = true;

    err = ep_recv_buf(ep_ctx, buf_info->shm_buf.data, transfer_size, buf_info);
    if (err) {
        ERROR("%s ep_recv_buf failed with errno: %s", __func__, fi_strerror(-err));
        return err;
    }
    return 0;
}

void RxRdmaSession::handle_received_buffers()
{
    shm_buf_info_t *buf_info;
    int err;
    uint16_t bursted_buf_num;

    err = ep_cq_read(ep_ctx, (void **)&buf_info, 1);
    if (err) {
        if (err != -EAGAIN)
            INFO("%s ep_rxcq_read: %s", __func__, strerror(-err));
        return;
    }
    fb_recv++;

    err = memif_tx_burst(memif_conn, 0, &buf_info->shm_buf, 1, &bursted_buf_num);
    if (err != MEMIF_ERR_SUCCESS && bursted_buf_num != 1) {
        INFO("%s memif_tx_burst: %s", __func__, memif_strerror(err));
        return;
    }
    buf_info->used = false;
}

void RxRdmaSession::frame_thread()
{
    while (!atomic_load_explicit(&shm_ready, std::memory_order_acquire) && !stop)
        usleep(1000);

    INFO("%s, RX RDMA thread started\n", __func__);
    while (!stop) {
        if (!atomic_load_explicit(&shm_ready, std::memory_order_acquire))
            continue;
        while (!pass_empty_buf_to_libfabric()) {
        }
        handle_received_buffers();
    }
}

RxRdmaSession::RxRdmaSession(libfabric_ctx *dev_handle, const mcm_conn_param &request,
                             memif_ops_t &memif_ops)
    : Session(memif_ops, request.payload_type, RX), ep_cfg{0}, ep_ctx(0), stop(false),
      frame_thread_handle(0), fb_recv(0), shm_bufs(0), shm_buf_num(0)
{
    transfer_size = request.payload_args.rdma_args.transfer_size;

    ep_cfg.rdma_ctx = dev_handle;
    memcpy(&ep_cfg.remote_addr, &request.remote_addr, sizeof(request.remote_addr));
    memcpy(&ep_cfg.local_addr, &request.local_addr, sizeof(request.local_addr));
    ep_cfg.dir = direction::RX;
}

int RxRdmaSession::init()
{
    int err = ep_init(&ep_ctx, &ep_cfg);
    if (err) {
        ERROR("Failed to initialize libfabric's end point");
        return -1;
    }

    shm_buf_num = 1 << 4;
    shm_bufs = (shm_buf_info_t *)calloc(shm_buf_num, sizeof(shm_buf_info_t));
    if (!shm_bufs) {
        ERROR("Failed to allocate memory");
        return -1;
    }

    err = shm_init(transfer_size, 4);
    if (err < 0) {
        ERROR("Failed to initialize shared memory");
        return -1;
    }

    frame_thread_handle = new std::thread(&RxRdmaSession::frame_thread, this);
    if (!frame_thread_handle) {
        ERROR("Failed to create thread");
        return -1;
    }
    return 0;
}

RxRdmaSession::~RxRdmaSession()
{
    INFO("%s, fb_recv %d\n", __func__, fb_recv);
    stop = true;
    if (ep_ctx) {
        if (ep_destroy(&ep_ctx)) {
            ERROR("Failed to destroy RDMA context");
        }
        ep_ctx = 0;
    }

    if (shm_bufs) {
        free(shm_bufs);
        shm_bufs = 0;
    }

    if (frame_thread_handle) {
        frame_thread_handle->join();
        delete frame_thread_handle;
        frame_thread_handle = 0;
    }
}

int RxRdmaSession::on_connect_cb(memif_conn_handle_t conn)
{
    memif_region_details_t region;

    int err = memif_get_buffs_region(conn, &region);
    if (err) {
        ERROR("%s, Getting memory buffers from memif failed. \n", __func__);
        return err;
    }

    err = ep_reg_mr(ep_ctx, region.addr, region.size);
    if (err) {
        ERROR("%s, ep_reg_mr failed: %s\n", __func__, fi_strerror(-err));
        return err;
    }

    return Session::on_connect_cb(conn);
}

int RxRdmaSession::on_disconnect_cb(memif_conn_handle_t conn)
{
    /* TODO: unregister in libfabric memory regions allocated by memif */

    return Session::on_disconnect_cb(conn);
}
