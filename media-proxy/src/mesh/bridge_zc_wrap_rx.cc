/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "multipoint_zc.h"
#include "bridge_zc_wrap_rx.h"
#include "logger.h"

namespace mesh::connection {

Result ZeroCopyWrapperBridgeRx::configure(context::Context& ctx,
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

Result ZeroCopyWrapperBridgeRx::set_link(context::Context& ctx, Connection *new_link,
                                         Connection *requester)
{
    log::debug("set_link ZC bridge Rx %p %p", new_link, requester);

    auto res = Connection::set_link(ctx, new_link, requester);
    if (res != Result::success)
        return res;

    return multipoint::zc_init_gateway_from_group(ctx, &gw, link());
}

Result ZeroCopyWrapperBridgeRx::on_establish(context::Context& ctx)
{
    bridge->set_link(ctx, this);

    set_state(ctx, State::active);
    return Result::success;
}

Result ZeroCopyWrapperBridgeRx::on_shutdown(context::Context& ctx)
{
    gw.shutdown(ctx);
    return bridge->shutdown(ctx);
}

Result ZeroCopyWrapperBridgeRx::on_receive(context::Context& ctx, void *ptr,
                                           uint32_t sz, uint32_t& sent)
{
    auto res = gw.transmit(ctx, ptr, sz, sent);
    switch (res) {
    case zerocopy::gateway::Result::success:
        return Result::success;
    case zerocopy::gateway::Result::error_wrong_state:
        return Result::error_wrong_state;
    case zerocopy::gateway::Result::error_context_cancelled:
        return Result::error_context_cancelled;
    default:
        return Result::error_general_failure;
    }
}

} // namespace mesh::connection
