/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_LOCAL_H
#define CONN_LOCAL_H

#include "conn.h"
#include "mcm_dp.h"
#include "shm_memif.h"
#include <cstdarg>

namespace mesh::connection {

class Local : public Connection {

public:
    Result configure_memif(context::Context& ctx, memif_ops_t *ops,
                           size_t frame_size);
    void get_params(memif_conn_param *param);

protected:
    virtual void default_memif_ops(memif_ops_t *ops) = 0;
    virtual int on_memif_receive(void *ptr, uint32_t sz) = 0;

    memif_socket_handle_t memif_socket;
    memif_conn_handle_t memif_conn;
    size_t frame_size;

private:
    Result on_establish(context::Context& ctx) override;
    Result on_shutdown(context::Context& ctx) override;

    static int callback_on_connect(memif_conn_handle_t conn, void *private_ctx);
    static int callback_on_disconnect(memif_conn_handle_t conn, void *private_ctx);
    static int callback_on_interrupt(memif_conn_handle_t conn, void *private_ctx,
                                     uint16_t qid);

    memif_socket_args_t memif_socket_args;
    memif_conn_args_t memif_conn_args;
    memif_ops_t ops;
    std::jthread th;
    bool ready;
};

} // namespace mesh::connection

#endif // CONN_LOCAL_H
