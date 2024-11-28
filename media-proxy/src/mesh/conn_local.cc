/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "conn_local.h"
#include "logger.h"
#include <bsd/string.h>
#include <sys/stat.h>

namespace mesh::connection {

Result Local::configure_memif(context::Context& ctx, memif_ops_t *ops,
                              size_t frame_size)
{
    this->frame_size = frame_size;

    bzero(memif_socket_args.app_name, sizeof(memif_socket_args.app_name));
    bzero(memif_socket_args.path, sizeof(memif_socket_args.path));

    if (!ops) {
        ops = &this->ops;
        default_memif_ops(ops);
    }

    // Set application name
    strlcpy(memif_socket_args.app_name, ops->app_name,
            sizeof(memif_socket_args.app_name));

    // Create memif socket.
    // Interfaces are internally stored in a database referenced by memif socket.
    strlcpy(memif_socket_args.path, ops->socket_path, sizeof(memif_socket_args.path));

    // Create memif interfaces
    // Both interaces are assigned the same socket and same id to create a loopback.
    ready = false;
    memif_conn_args.interface_id = ops->interface_id;
    memif_conn_args.buffer_size = (uint32_t)frame_size;
    memif_conn_args.log2_ring_size = 2;
    snprintf((char*)memif_conn_args.interface_name,
             sizeof(memif_conn_args.interface_name), "%s", ops->interface_name);
    memif_conn_args.is_master = 1;

    set_state(ctx, State::configured);
    return Result::success;
}

void Local::get_params(memif_conn_param *param)
{
    if (param) {
        memcpy(&param->socket_args, &memif_socket_args, sizeof(param->socket_args));
        memcpy(&param->conn_args, &memif_conn_args, sizeof(param->conn_args));
    }
}

Result Local::on_establish(context::Context& ctx)
{
    // Unlink socket file
    if (memif_socket_args.path[0] != '@') {
        auto err = mkdir("/run/mcm", 0666);
        if (err && errno != EEXIST) {
            return Result::error_general_failure;
        }
        unlink(memif_socket_args.path);
    }

    auto ret = memif_create_socket(&memif_socket, &memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        log::error("memif_create_socket: %s", memif_strerror(ret));
        return Result::error_general_failure;
    }

    memif_conn_args.socket = memif_socket;

    log::debug("Create memif interface.");
    ret = memif_create(&memif_conn, &memif_conn_args,
                       Local::callback_on_connect,
                       Local::callback_on_disconnect,
                       Local::callback_on_interrupt, this);
    if (ret != MEMIF_ERR_SUCCESS) {
        log::error("memif_create: %s", memif_strerror(ret));
        return Result::error_general_failure;
    }

    // Start the memif event loop.
    // TODO: Replace ctx with a context passed at creation.
    try {
        th = std::jthread([this]() {
            for (;;) {
                int err = memif_poll_event(memif_socket, -1);
                if (err)
                    break;
            }
        });
    }
    catch (const std::system_error& e) {
        log::error("thread create failed (%d)", ret);
        return Result::error_out_of_memory;
    }

    set_state(ctx, State::active);
    return Result::success;
}

int Local::callback_on_connect(memif_conn_handle_t conn, void *private_ctx)
{
    auto _this = static_cast<Local *>(private_ctx);
    if (!_this)
        return MEMIF_ERR_INVAL_ARG;

    int err = memif_refill_queue(_this->memif_conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        log::error("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    _this->ready = true;

    print_memif_details(_this->memif_conn);

    log::debug("Memif ready");

    return MEMIF_ERR_SUCCESS;
}

int Local::callback_on_disconnect(memif_conn_handle_t conn, void *private_ctx)
{
    auto _this = static_cast<Local *>(private_ctx);
    if (!_this)
        return MEMIF_ERR_INVAL_ARG;

    if (!_this->ready)
        return MEMIF_ERR_SUCCESS;

    _this->ready = false;

    auto err = memif_cancel_poll_event(_this->memif_socket);
    if (err != MEMIF_ERR_SUCCESS)
        log::error("on_disconnect memif_cancel_poll_event: %s",
                   memif_strerror(err));

    return MEMIF_ERR_SUCCESS;
}

int Local::callback_on_interrupt(memif_conn_handle_t conn, void *private_ctx,
                                 uint16_t qid)
{
    auto _this = static_cast<Local *>(private_ctx);
    if (!_this)
        return MEMIF_ERR_INVAL_ARG;

    int err = 0;
    memif_buffer_t shm_bufs = { 0 };
    uint16_t buf_num = 0;

    if (!_this->ready) {
        log::warn("Memif conn already stopped.");
        return -1;
    }

    // Receive packets from the shared memory
    err = memif_rx_burst(_this->memif_conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        log::error("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    _this->on_memif_receive(shm_bufs.data, shm_bufs.len);

    err = memif_refill_queue(_this->memif_conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        log::error("memif_refill_queue: %s", memif_strerror(err));

    return 0;
}

Result Local::on_shutdown(context::Context& ctx)
{
    log::debug("Memif shutdown");

    auto err = memif_cancel_poll_event(memif_socket);
    if (err != MEMIF_ERR_SUCCESS) {
        log::error("on_shutdown memif_cancel_poll_event: %s",
                   memif_strerror(err));
    }

    th.join();

    // Free-up resources
    memif_delete(&memif_conn);
    memif_delete_socket(&memif_socket);

    // Unlink socket file
    if (memif_socket_args.path[0] != '@')
        unlink(memif_socket_args.path);

    uint64_t in = metrics.inbound_bytes;
    uint64_t out = metrics.outbound_bytes;

    log::info("Local %s conn shutdown", kind2str(_kind, true))
             ("frames", metrics.transactions_successful)
             ("in", in)("out", out)("equal", in == out);

    uint64_t errors = metrics.errors;
    uint64_t failures = metrics.transactions_failed;

    if (errors || failures)
        log::error("Local %s conn shutdown", kind2str(_kind, true))
                  ("frames_failed", failures)
                  ("errors", errors);

    set_state(ctx, State::closed);
    return Result::success;
}

} // namespace mesh::connection
