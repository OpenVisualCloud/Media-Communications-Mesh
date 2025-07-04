/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "multipoint_zc.h"
#include "bridge_zc_wrap_tx.h"
#include "logger.h"

namespace mesh::connection {

Result ZeroCopyWrapperBridgeTx::configure(context::Context& ctx,
                                          const BridgeConfig& cfg)
{
    BridgeConfig new_cfg = cfg;
    new_cfg.conn_config.options.engine = "";

    auto err = bridges_manager.make_bridge(ctx, bridge, new_cfg);
    if (err) {
        set_state(ctx, State::not_configured);
        return Result::error_general_failure;
    }

    set_state(ctx, State::configured);
    return Result::success;
}

Result ZeroCopyWrapperBridgeTx::set_link(context::Context& ctx, Connection *new_link,
                                         Connection *requester)
{
    log::debug("set_link ZC bridge Tx %p %p", new_link, requester);

    auto res = Connection::set_link(ctx, new_link, requester);
    if (res != Result::success)
        return res;

    return multipoint::zc_init_gateway_from_group(ctx, &gw, link());
}

Result ZeroCopyWrapperBridgeTx::on_establish(context::Context& ctx)
{
    set_state(ctx, State::active);

    gw.set_tx_callback([this](context::Context& ctx, void *ptr, uint32_t sz,
                              uint32_t& sent) -> zerocopy::gateway::Result {
        metrics.inbound_bytes += sz;

        auto res = bridge->do_receive(ctx, ptr, sz, sent);

        metrics.outbound_bytes += sent;

        if (res == Result::success)
            metrics.transactions_succeeded++;
        else
            metrics.transactions_failed++;

        switch (res) {
        case Result::success:
            return zerocopy::gateway::Result::success;
        case Result::error_wrong_state:
            return zerocopy::gateway::Result::error_wrong_state;
        case Result::error_context_cancelled:
            return zerocopy::gateway::Result::error_context_cancelled;
        default:
            return zerocopy::gateway::Result::error_general_failure;
        }
    });

    return Result::success;
}

Result ZeroCopyWrapperBridgeTx::on_shutdown(context::Context& ctx)
{
    gw.shutdown(ctx);
    return bridge->shutdown(ctx);
}

} // namespace mesh::connection
