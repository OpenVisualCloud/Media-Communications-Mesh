/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "multipoint_copy.h"
#include "logger.h"

namespace mesh::multipoint {

Result CopyGroup::on_establish(context::Context& ctx)
{
    set_state(ctx, State::active);
    set_status(ctx, Status::healthy);

    return Result::success;
}

std::list<Connection *> * CopyGroup::get_hotpath_outputs_lock()
{
    return reinterpret_cast<std::list<Connection *> *>(outputs_ptr.load_next_lock());
}

void CopyGroup::hotpath_outputs_unlock() {
    outputs_ptr.unlock();
}

void CopyGroup::set_hotpath_outputs(std::list<Connection *> *new_outputs)
{
    if (new_outputs)
        new_outputs = new std::list<Connection *>(*new_outputs);

    auto prev_outputs_ptr = reinterpret_cast<std::list<Connection *> *>(outputs_ptr.load());
    
    outputs_ptr.store_wait(new_outputs);
    
    if (prev_outputs_ptr)
        delete prev_outputs_ptr;
}

Result CopyGroup::on_receive(context::Context& ctx, void *ptr,
                             uint32_t sz, uint32_t& sent)
{
    if (state() != State::active)
        return set_result(Result::error_wrong_state);

    if (!link())
        return set_result(Result::error_no_link_assigned);

    metrics.inbound_bytes += sz;

    auto res = Result::success;
    uint32_t total_sent = 0;
    uint32_t errors = 0;    

    auto _outputs = get_hotpath_outputs_lock();

    if (!_outputs || _outputs->empty()) {
        hotpath_outputs_unlock();
        return Result::error_no_link_assigned;
    }

    for (Connection *output : *_outputs) {
        if (!output) {
            errors++;
            continue;
        }

        uint32_t out_sent = 0;
        res = output->do_receive(ctx, ptr, sz, out_sent);

        total_sent += out_sent;

        if (res != Result::success)
            errors++;
    }

    hotpath_outputs_unlock();

    sent = sz;
    metrics.outbound_bytes += total_sent;
    metrics.errors += errors;

    if (!errors)
        metrics.transactions_succeeded++;
    else
        metrics.transactions_failed++;

    return Result::success;
}

void CopyGroup::on_outputs_updated()
{
    set_hotpath_outputs(outputs.empty() ? nullptr : &outputs);
}

} // namespace mesh::multipoint
