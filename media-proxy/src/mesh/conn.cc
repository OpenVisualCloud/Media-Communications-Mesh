/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "conn.h"
#include <cstring>

namespace mesh::connection {

Connection::Connection()
{
    std::memset(&metrics, 0, sizeof(metrics));
}

Connection::~Connection()
{
    auto ctx = context::WithTimeout(context::Background(),
                                    std::chrono::milliseconds(5000));
    set_state(ctx, State::deleting);
    on_delete(ctx);
}

Kind Connection::kind()
{
    return _kind;
}

void Connection::set_state(context::Context& ctx, State new_state)
{
    State old_state = _state.exchange(new_state, std::memory_order_release);
    if (old_state != new_state) {
        // TODO: generate an Event. Use context to cancel sending the Event.
    }
}

State Connection::state()
{
    return _state.load(std::memory_order_acquire);
}

void Connection::set_status(context::Context& ctx, Status new_status)
{
    Status old_status = _status.exchange(new_status, std::memory_order_release);
    if (old_status != new_status) {
        // TODO: generate an Events. Use context to cancel sending the Event.
    }
}

Status Connection::status()
{
    // TODO: Rethink the logic here.

    switch (state()) {
    case State::not_configured:
    case State::configured:
        return Status::initial;
    case State::establishing:
    case State::closing:
        return Status::transition;
    case State::closed:
    case State::deleting:
        return Status::shutdown;
    default:
        return _status.load(std::memory_order_acquire);
    }
}

Result Connection::establish(context::Context& ctx)
{
    switch (state()) {
    case State::configured:
    case State::closed:
        set_state(ctx, State::establishing);
        return set_result(on_establish(ctx));
    default:
        return set_result(Result::error_wrong_state);
    }
}

Result Connection::suspend(context::Context& ctx)
{
    if (state() != State::active)
        return set_result(Result::error_wrong_state);

    set_state(ctx, State::suspended);
    return set_result(Result::success);
}

Result Connection::resume(context::Context& ctx)
{
    if (state() != State::suspended)
        return set_result(Result::error_wrong_state);

    set_state(ctx, State::active);
    return set_result(Result::success);
}

Result Connection::shutdown(context::Context& ctx)
{
    switch (state()) {
    case State::closed:
        return set_result(Result::success);
    case State::deleting:
        return set_result(Result::error_wrong_state);
    default:
        set_state(ctx, State::closing);
        return set_result(on_shutdown(ctx));
    }
}

Result Connection::transmit(context::Context& ctx, void *ptr, uint32_t sz)
{
    // WARNING: This is the hot path of Data Plane.
    // Avoid any unnecessary operations that can increase latency.

    if (state() != State::active)
        return set_result(Result::error_wrong_state);

    if (!_link)
        return set_result(Result::error_no_link_assigned);

    metrics.inbound_bytes += sz;

    transmitting.store(true, std::memory_order_release);
    setting_link.wait(true, std::memory_order_acquire);

    uint32_t sent = 0;
    Result res = _link->do_receive(ctx, ptr, sz, sent);

    transmitting.store(false, std::memory_order_release);
    transmitting.notify_one();

    metrics.outbound_bytes += sent;

    if (res == Result::success)
        metrics.transactions_succeeded++;
    else
        metrics.transactions_failed++;

    return res;
}

Result Connection::do_receive(context::Context& ctx, void *ptr, uint32_t sz,
                              uint32_t& sent)
{
    // WARNING: This is the hot path of Data Plane.
    // Avoid any unnecessary operations that can increase latency.

    metrics.inbound_bytes += sz;

    if (state() != State::active)
        return Result::error_wrong_state;

    Result res = on_receive(ctx, ptr, sz, sent);

    metrics.outbound_bytes += sent;
    if (res == Result::success)
        metrics.transactions_succeeded++;
    else
        metrics.transactions_failed++;

    return res;
}

Result Connection::on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                              uint32_t& sent)
{
    // This is the default implementation.
    // Derived Tx connection classes must override this method.
    //
    // WARNING: This is the hot path of Data Plane.
    // The method must return as soon as possible.
    // Avoid any unnecessary operations that can increase latency.

    return Result::error_not_supported;
}

Result Connection::set_link(context::Context& ctx, Connection *new_link,
                            Connection *requester)
{
    (void)requester;

    // WARNING: Changing a link will affect the hot path of Data Plane.
    // Avoid any unnecessary operations that can increase latency.

    if (_link == new_link)
        return set_result(Result::success);

    // TODO: generate an Event (conn_changing_link).
    // Use context to cancel sending the Event.

    setting_link.store(true, std::memory_order_release);
    transmitting.wait(true, std::memory_order_acquire);

    _link = new_link;

    setting_link.store(false, std::memory_order_release);
    setting_link.notify_one();

    // TODO: generate a post Event (conn_link_changed).
    // Use context to cancel sending the Event.

    return set_result(Result::success); // TODO: return error if needed
}

Connection * Connection::link()
{
    return _link;
}

Result Connection::set_result(Result res)
{
    if (res != Result::success)
        metrics.errors++;

    return res;
}

void Connection::collect(telemetry::Metric& metric, const int64_t& timestamp_ms)
{
    uint64_t in = metrics.inbound_bytes;
    uint64_t out = metrics.outbound_bytes;
    uint32_t strn = metrics.transactions_succeeded;

    metric.addFieldString("state", state2str(state()));
    metric.addFieldBool("link", link() != nullptr);
    metric.addFieldUint64("in", in);
    metric.addFieldUint64("out", out);
    metric.addFieldUint64("strn", strn);
    metric.addFieldUint64("ftrn", metrics.transactions_failed);
    metric.addFieldUint64("err", metrics.errors);

    auto dt = timestamp_ms - metrics.prev_timestamp_ms;

    if (metrics.prev_inbound_bytes) {
        uint64_t bw = (in - metrics.prev_inbound_bytes);
        bw = (bw * 8 * 1000) / dt;
        metric.addFieldDouble("inbw", (bw/1000)/1000.0);
    }
    metrics.prev_inbound_bytes = in;

    if (metrics.prev_outbound_bytes) {
        uint64_t bw = (out - metrics.prev_outbound_bytes);
        bw = (bw * 8 * 1000) / dt;
        metric.addFieldDouble("outbw", (bw/1000)/1000.0);
    }
    metrics.prev_outbound_bytes = out;

    if (metrics.prev_transactions_succeeded) {
        uint64_t tps = (strn - metrics.prev_transactions_succeeded);
        tps = (tps * 10 * 1000) / dt;
        metric.addFieldDouble("tps", tps/10.0);
    }
    metrics.prev_transactions_succeeded = strn;

    metrics.prev_timestamp_ms = timestamp_ms;

    auto errors_delta = metrics.errors - metrics.prev_errors;
    metrics.prev_errors = metrics.errors;
    metric.addFieldUint64("errd", errors_delta);
}

static const char str_unknown[] = "?unknown?";

const char * kind2str(Kind kind, bool brief)
{
    switch (kind) {
    case Kind::undefined:   return brief ? "Undef" : "undefined";
    case Kind::transmitter: return brief ? "Tx" : "transmitter";
    case Kind::receiver:    return brief ? "Rx" : "receiver";
    default:                return str_unknown;
    }
}

const char * state2str(State state)
{
    switch (state) {
    case State::not_configured: return "not configured";
    case State::configured:     return "configured";
    case State::establishing:   return "establishing";
    case State::active:         return "active";
    case State::suspended:      return "suspended";
    case State::closing:        return "closing";
    case State::closed:         return "closed";
    case State::deleting:       return "deleting";
    default:                    return str_unknown;
    }
}

const char * status2str(Status status)
{
    switch (status) {
    case Status::initial:    return "initial";
    case Status::transition: return "transition";
    case Status::healthy:    return "healthy";
    case Status::failure:    return "failure";
    case Status::shutdown:   return "shutdown";
    default:                 return str_unknown;
    }
}

const char * result2str(Result res)
{
    switch (res) {
    case Result::success:                return "success";
    case Result::error_not_supported:    return "not supported";
    case Result::error_wrong_state:      return "wrong state";
    case Result::error_no_link_assigned: return "no link assigned";
    case Result::error_bad_argument:     return "bad argument";
    case Result::error_out_of_memory:    return "out of memory";
    case Result::error_general_failure:  return "general failure";
    case Result::error_context_cancelled:          return "context cancelled";
    case Result::error_already_initialized:        return "already initialized";
    case Result::error_initialization_failed:      return "initialization failed";
    case Result::error_memory_registration_failed: return "memory registration failed";
    case Result::error_thread_creation_failed:     return "thread creation failed";
    case Result::error_no_buffer:        return "no buffer";
    case Result::error_timeout:          return "timeout";
    default:                             return str_unknown;
    }
}

} // namespace mesh::connection
