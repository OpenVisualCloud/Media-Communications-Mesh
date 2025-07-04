/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "multipoint.h"
#include "logger.h"

namespace mesh::multipoint {

Group::Group(const std::string& id) : Connection()
{
    _kind = Kind::transmitter;
    assign_id(id);
}

void Group::configure(context::Context& ctx)
{
    set_state(ctx, State::configured);
}

Result Group::set_link(context::Context& ctx, Connection *new_link,
                       Connection *requester)
{
    if (!new_link && requester) {
        // Remove the requester as the group input
        if (requester == link()) {
            log::info("[GROUP] Remove input")("group_id", id)("id", requester->id);
            return Connection::set_link(ctx, nullptr);
        }

        // Remove the requester from the group outputs list
        for (auto it = outputs.begin(); it != outputs.end(); ++it) {
            if (*it != requester)
                continue;

            log::info("[GROUP] Delete output")("group_id", id)("id", requester->id);

            {
                const std::lock_guard<std::mutex> lk(outputs_mx);

                outputs.erase(it);
            }
            break;
        }

        on_outputs_updated();

        return Result::success;
    }

    // log::info("[GROUP] Set link")("group_id", id)("new_link", new_link)
    //                             ("requester", requester);
    return Connection::set_link(ctx, new_link);
}

bool Group::input_assigned()
{
    return link() != nullptr;
}

Result Group::assign_input(context::Context& ctx, Connection *input)
{
    if (input->kind() != Kind::receiver)
        return Result::error_bad_argument;

    log::info("[GROUP] Assign input")("group_id", id)("id", input->id);

    return set_link(ctx, input);
}

Result Group::add_output(context::Context& ctx, Connection *output)
{
    if (output->kind() != Kind::transmitter)
        return Result::error_bad_argument;

    log::info("[GROUP] Add output")("group_id", id)("id", output->id);

    {
        const std::lock_guard<std::mutex> lk(outputs_mx);

        outputs.emplace_back(output);
    }    

    on_outputs_updated();

    return Result::success;
}

int Group::outputs_num()
{
    return outputs.size();
}

Result Group::on_shutdown(context::Context& ctx)
{
    if (link()) {
        link()->set_link(ctx, nullptr);
        set_link(ctx, nullptr);
    }

    outputs.clear();
    on_outputs_updated();

    set_state(ctx, State::closed);
    set_status(ctx, Status::shutdown);

    return Result::success;
}

} // namespace mesh::multipoint
