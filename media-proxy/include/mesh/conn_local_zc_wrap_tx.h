/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_LOCAL_ZC_WRAP_TX_H
#define CONN_LOCAL_ZC_WRAP_TX_H

#include "conn_local_tx.h"
#include "gateway_zc.h"

namespace mesh::connection {

using namespace zerocopy::gateway;

class ZeroCopyWrapperLocalTx : public LocalTx {
public:
    ZeroCopyWrapperLocalTx() : LocalTx() {}

    Result set_link(context::Context& ctx, Connection *new_link,
                    Connection *requester = nullptr) override;

private:
    Result on_establish(context::Context& ctx) override;
    Result on_shutdown(context::Context& ctx) override;

    GatewayTx gw;
};

} // namespace mesh::connection

#endif // CONN_LOCAL_ZC_WRAP_TX_H
