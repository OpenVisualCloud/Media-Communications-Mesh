/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BRIDGE_ZC_WRAP_TX_H
#define BRIDGE_ZC_WRAP_TX_H

#include "conn.h"
#include "gateway_zc.h"
#include "manager_bridges.h"

namespace mesh::connection {

using namespace zerocopy::gateway;

class ZeroCopyWrapperBridgeTx : public Connection {
public:
    ZeroCopyWrapperBridgeTx() : Connection() { _kind = Kind::transmitter; }

    Result configure(context::Context& ctx, const BridgeConfig& cfg);

    Result set_link(context::Context& ctx, Connection *new_link,
                    Connection *requester = nullptr) override;

private:
    Result on_establish(context::Context& ctx) override;
    Result on_shutdown(context::Context& ctx) override;

    Connection *bridge;
    GatewayTx gw;
};

} // namespace mesh::connection

#endif // BRIDGE_ZC_WRAP_TX_H
