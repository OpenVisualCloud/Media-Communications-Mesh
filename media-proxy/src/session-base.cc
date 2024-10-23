/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include <mcm_dp.h>

#include "session-base.h"
#include <unistd.h>

static int on_connect_callback_wrapper(memif_conn_handle_t conn, void *priv_data)
{
    session *connection = (session *)priv_data;
    return connection->on_connect_cb(conn);
}

static int on_disconnect_callback_wrapper(memif_conn_handle_t conn, void *priv_data)
{
    session *connection = (session *)priv_data;
    return connection->on_disconnect_cb(conn);
}

static int on_receive_callback_wrapper(memif_conn_handle_t conn, void *priv_data, uint16_t qid)
{
    session *connection = (session *)priv_data;
    return connection->on_receive_cb(conn, qid);
}

void session::memif_event_loop()
{
    int err;
    memif_socket_handle_t memif_socket = memif_conn_args.socket;

    do {
        // INFO("media-proxy waiting event.");
        err = memif_poll_event(memif_socket, -1);
        // INFO("media-proxy received event.");
    } while (err == MEMIF_ERR_SUCCESS);

    INFO("MEMIF DISCONNECTED.");
}

int session::shm_init(memif_ops_t *memif_ops, uint32_t buffer_size, uint32_t log2_ring_size)
{
    int ret = 0;

    /* Set application name */
    strncpy(memif_socket_args.app_name, memif_ops->app_name,
            sizeof(memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(memif_socket_args.path, memif_ops->socket_path, sizeof(memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && memif_socket_args.path[0] != '@') {
        struct stat st = {0};
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&memif_socket, &memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    shm_ready = ATOMIC_VAR_INIT(false);
    memif_conn_args.socket = memif_socket;
    memif_conn_args.interface_id = memif_ops->interface_id;
    memif_conn_args.buffer_size = buffer_size;
    memif_conn_args.log2_ring_size = log2_ring_size;
    memcpy((char *)memif_conn_args.interface_name, memif_ops->interface_name,
           sizeof(memif_conn_args.interface_name));
    memif_conn_args.is_master = memif_ops->is_master;

    INFO("create memif interface.");
    ret = memif_create(&memif_conn, &memif_conn_args, on_connect_callback_wrapper,
                       on_disconnect_callback_wrapper, on_receive_callback_wrapper, this);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        return -1;
    }

    /* Start the MemIF event loop. */
    memif_event_thread = new std::thread(&session::memif_event_loop, this);
    if (!memif_event_thread) {
        ERROR("%s(%d), thread create fail\n", __func__, ret);
        return -1;
    }

    return 0;
}

int session::shm_deinit()
{
    int err = 0;

    if (memif_event_thread) {
        memif_event_thread->join();
        delete memif_event_thread;
        memif_event_thread = 0;
    }

    /* free-up resources */
    memif_delete(&memif_conn);
    memif_delete_socket(&memif_socket);

    /* unlink socket file */
    if (memif_conn_args.is_master && memif_socket_args.path[0] != '@') {
        unlink(memif_socket_args.path);
    }

    return 0;
}

int session::on_connect_cb(memif_conn_handle_t conn)
{
    INFO("RX RDMA memif connected!");

    int err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    atomic_store_explicit(&shm_ready, true, memory_order_release);

    return 0;
}

int session::on_disconnect_cb(memif_conn_handle_t conn)
{
    if (!conn) {
        INFO("Invalid Parameters.");
        return -EINVAL;
    }

    /* release session */
    if (!atomic_load_explicit(&shm_ready, memory_order_relaxed)) {
        return 0;
    }
    atomic_store_explicit(&shm_ready, false, memory_order_relaxed);

    /* stop event polling thread */
    INFO("Stop poll event\n");
    memif_socket_handle_t socket = memif_get_socket_handle(conn);
    if (!socket) {
        INFO("Invalide socket handle.");
        return -1;
    }

    int err = memif_cancel_poll_event(socket);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("We are doomed...");
    }

    return 0;
}

int session::on_receive_cb(memif_conn_handle_t conn, uint16_t qid)
{
    memif_buffer_t shm_bufs;
    uint16_t buf_num = 0;

    /* receive packets from the shared memory */
    int err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    /* Process on the received buffer. */
    /* Supposed this function will never be called. */

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    return 0;
}

session::session() : id(0), memif_socket(0), memif_conn(0)
{
    memif_event_thread = 0;
    type = TX;
    payload_type = PAYLOAD_TYPE_NONE;
    memif_socket_args = {0};
    memif_conn_args = {0};
}

session::~session() { shm_deinit(); }
