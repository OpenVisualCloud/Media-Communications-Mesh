/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SESSION_BASE_H
#define __SESSION_BASE_H

#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <bsd/string.h>

#include <mcm_dp.h>

#include "shm_memif.h" /* shared memory */
#include "utils.h"

class Session
{
    uint32_t id;
    enum direction type;
    mcm_payload_type payload_type;

    /* shared memory arguments */
    memif_socket_handle_t memif_socket;
    memif_socket_args_t memif_socket_args;
    memif_conn_args_t memif_conn_args;
    std::thread *memif_event_thread;

    void memif_event_loop();
    int shm_deinit();

  protected:
    memif_conn_handle_t memif_conn;
    std::atomic_bool shm_ready;

    int shm_init(uint32_t buffer_size, uint32_t log2_ring_size);
    Session(memif_ops_t &memif_ops, mcm_payload_type payload, direction dir_type);

  public:
    uint32_t get_id() { return id; };
    memif_socket_args_t get_socket_args() { return memif_socket_args; };
    memif_conn_args_t get_conn_args() { return memif_conn_args; };

    virtual ~Session();

    virtual int init() = 0;
    virtual int on_connect_cb(memif_conn_handle_t conn);
    virtual int on_disconnect_cb(memif_conn_handle_t conn);
    virtual int on_receive_cb(memif_conn_handle_t conn, uint16_t qid);
};

#endif /* __SESSION_BASE_H */
