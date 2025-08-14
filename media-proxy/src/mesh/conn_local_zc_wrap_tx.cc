/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "multipoint_zc.h"
#include "conn_local_zc_wrap_tx.h"
#include "logger.h"

namespace mesh::connection {

Result ZeroCopyWrapperLocalTx::set_link(context::Context& ctx, Connection *new_link,
                                        Connection *requester)
{
    log::debug("set_link ZC wrapper LocalTx %p %p", new_link, requester);

    auto res = Connection::set_link(ctx, new_link, requester);
    if (res != Result::success)
        return res;

    return multipoint::zc_init_gateway_from_group(ctx, &gw, link());
}

Result ZeroCopyWrapperLocalTx::on_establish(context::Context& ctx)
{
    auto res = Local::on_establish(ctx);
    if (res != Result::success)
        return res;

    gw.set_tx_callback([this](context::Context& ctx, void *ptr, uint32_t sz,
                              uint32_t& sent) -> zerocopy::gateway::Result {
        auto res = this->do_receive(ctx, ptr, sz, sent);
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

    return set_result(Result::success);
}

Result ZeroCopyWrapperLocalTx::on_shutdown(context::Context& ctx)
{
    gw.shutdown(ctx);
    return Local::on_shutdown(ctx);
}

} // namespace mesh::connection
