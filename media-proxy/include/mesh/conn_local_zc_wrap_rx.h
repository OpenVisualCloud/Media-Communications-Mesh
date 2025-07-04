/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_LOCAL_ZC_WRAP_RX_H
#define CONN_LOCAL_ZC_WRAP_RX_H

#include "conn_local_rx.h"
#include "gateway_zc.h"

namespace mesh::connection {

using namespace zerocopy::gateway;

class ZeroCopyWrapperLocalRx : public Connection {
public:
    ZeroCopyWrapperLocalRx() : Connection() { _kind = Kind::receiver; }

    void configure(context::Context& ctx);

    LocalRx * get_memif_conn() { return &local_rx; }

    Result set_link(context::Context& ctx, Connection *new_link,
                    Connection *requester = nullptr) override;

private:
    Result on_establish(context::Context& ctx) override;
    Result on_resume(context::Context& ctx) override;
    Result on_shutdown(context::Context& ctx) override;

    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                      uint32_t& sent) override;

    LocalRx local_rx;
    GatewayRx gw;
};

} // namespace mesh::connection

#endif // CONN_LOCAL_ZC_WRAP_RX_H
