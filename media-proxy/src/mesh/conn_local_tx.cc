/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "conn_local_tx.h"
#include <string.h>
#include "logger.h"

namespace mesh::connection {

LocalTx::LocalTx() : Local()
{
    _kind = Kind::transmitter;
}

void LocalTx::default_memif_ops(memif_ops_t *ops)
{
    strncpy(ops->app_name, "mcm_rx", sizeof(ops->app_name));
    strncpy(ops->interface_name, "mcm_rx", sizeof(ops->interface_name));
    strncpy(ops->socket_path, "/run/mcm/mcm_rx_memif.sock", sizeof(ops->socket_path));
}

int LocalTx::on_memif_receive(void *ptr, uint32_t sz)
{
    log::warn("SHOULD NEVER HAPPEN on_memif_receive %d", sz);
    return 0;
}

Result LocalTx::on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                           uint32_t& sent)
{
    uint16_t qid = 0;
    uint16_t buf_num = 1;
    uint16_t rx_buf_num = 0, rx = 0;
    uint32_t buf_size = frame_size;
    memif_buffer_t shm_bufs = {};

    int err = memif_buffer_alloc_timeout(memif_conn, qid, &shm_bufs, buf_num,
                                         &rx_buf_num, buf_size, 10);
    if (err != MEMIF_ERR_SUCCESS) {
        log::error("rx_st20p_consume_frame: Failed to alloc memif buffer: %s",
                   memif_strerror(err));
        return Result::error_general_failure;
    }

    if (!shm_bufs.data) {
        log::error("Local Tx: shm_bufs.data == NULL");
        return Result::error_general_failure;
    }

    memcpy(shm_bufs.data, ptr, sz);
    sent = sz;

    // Send to microservice application
    err = memif_tx_burst(memif_conn, qid, &shm_bufs, rx_buf_num, &rx);
    if (err != MEMIF_ERR_SUCCESS) {
        log::error("rx_st20p_consume_frame memif_tx_burst: %s", memif_strerror(err));
    }

    return Result::success;
}

} // namespace mesh::connection
