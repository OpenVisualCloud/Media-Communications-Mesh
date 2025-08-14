/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "multipoint_zc.h"
#include "conn_local_zc.h"
#include "logger.h"
#include "event.h"

namespace mesh::connection {

Result ZeroCopyLocal::configure(context::Context& ctx)
{
    switch (config.kind) {
    case sdk::ConnectionKind::CONN_KIND_TRANSMITTER:
        _kind = Kind::transmitter;
        break;

    case sdk::ConnectionKind::CONN_KIND_RECEIVER:
        _kind = Kind::receiver;
        break;
    }

    set_state(ctx, State::configured);

    return Result::success;
}

Result ZeroCopyLocal::set_link(context::Context& ctx, Connection *new_link,
                               Connection *requester)
{
    log::debug("set_link ZC local %p %p", new_link, requester);

    auto res = Connection::set_link(ctx, new_link, requester);
    if (res != Result::success)
        return res;

    auto zc_group = dynamic_cast<multipoint::ZeroCopyGroup *>(link());
    if (!zc_group)
        return Result::error_bad_argument;

    auto group_cfg = zc_group->get_config();
    if (!group_cfg)
        return Result::error_general_failure;

    event::broker.send(ctx, parent(), event::Type::conn_zero_copy_config,
                       {{"conn_id", id},
                        {"temporary_id", sdk_temporary_id},
                        {"sysv_key", group_cfg->sysv_key},
                        {"mem_region_sz", group_cfg->mem_region_sz}});

    return Result::success;
}

Result ZeroCopyLocal::on_establish(context::Context& ctx)
{
    set_state(ctx, State::suspended);
    return Result::success;
}

Result ZeroCopyLocal::on_shutdown(context::Context& ctx)
{
    return Result::success;
}

} // namespace mesh::connection
