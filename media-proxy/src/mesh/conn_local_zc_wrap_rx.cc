/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "multipoint_zc.h"
#include "conn_local_zc_wrap_rx.h"
#include "logger.h"

namespace mesh::connection {

void ZeroCopyWrapperLocalRx::configure(context::Context& ctx)
{
    set_state(ctx, State::configured);
}

Result ZeroCopyWrapperLocalRx::set_link(context::Context& ctx, Connection *new_link,
                                        Connection *requester)
{
    log::debug("set_link ZC LocalRx %p %p", new_link, requester);

    auto res = Connection::set_link(ctx, new_link, requester);
    if (res != Result::success)
        return res;

    return multipoint::zc_init_gateway_from_group(ctx, &gw, link());
}

Result ZeroCopyWrapperLocalRx::on_establish(context::Context& ctx)
{
    set_state(ctx, State::suspended);

    local_rx.set_link(ctx, this);

    return local_rx.establish(ctx);
}

Result ZeroCopyWrapperLocalRx::on_resume(context::Context& ctx)
{
    return local_rx.resume(ctx);
}

Result ZeroCopyWrapperLocalRx::on_shutdown(context::Context& ctx)
{
    gw.shutdown(ctx);
    return local_rx.shutdown(ctx);
}

Result ZeroCopyWrapperLocalRx::on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                                          uint32_t& sent)
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
