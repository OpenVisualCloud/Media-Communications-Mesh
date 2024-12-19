/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_H
#define CONN_H

#include <atomic>
#include <cstddef>
#include "concurrency.h"
#include "metrics.h"

namespace mesh::connection {

/**
 * Kind
 * 
 * Definition of connection kinds.
 */
enum class Kind {
    undefined,
    transmitter,
    receiver,
};

/**
 * State
 * 
 * Definition of connection states.
 */
enum class State {
    not_configured, // set in ctor after initialization
    configured,
    establishing,
    active,
    suspended,
    closing,
    closed,
    deleting,       // set in dtor before deinitialization
};

/**
 * Status
 * 
 * Definition of connection statuses.
 */
enum class Status {
    initial,    // reported by the base Connection class
    transition, // reported by the base Connection class
    healthy,    // must be reported by the derived class
    failure,    // must be reported by the derived class
    shutdown,   // reported by the base Connection class
};

/**
 * Result
 * 
 * Definition of connection result options.
 */
enum class Result {
    success,
    error_not_supported,
    error_wrong_state,
    error_no_link_assigned,
    error_bad_argument,
    error_out_of_memory,
    error_general_failure,
    error_context_cancelled,
   
    error_already_initialized,
    error_initialization_failed,
    error_memory_registration_failed,
    error_thread_creation_failed,
    error_no_buffer,
    error_timeout,
    // TODO: more error codes to be added...
};

/**
 * Connection
 * 
 * Base abstract class of connection. All connection implementations must
 * inherit this class.
 */
class Connection : public telemetry::MetricsProvider {

public:
    Connection();
    virtual ~Connection();

    Kind kind();
    State state();
    Status status();

    virtual Result set_link(context::Context& ctx, Connection *new_link,
                            Connection *requester = nullptr);
    Connection * link();

    Result establish(context::Context& ctx);
    Result establish_async(context::Context& ctx);

    Result suspend(context::Context& ctx);
    Result resume(context::Context& ctx);

    Result shutdown(context::Context& ctx);
    Result shutdown_async(context::Context& ctx);

    Result do_receive(context::Context& ctx, void *ptr, uint32_t sz,
                      uint32_t& sent);

    // TODO: add calls to reset metrics (counters).

    struct {
        // TODO: add timestamp created_at
        // TODO: add timestamp established_at
    } info;

protected:
    void set_state(context::Context& ctx, State new_state);
    void set_status(context::Context& ctx, Status new_status);
    Result set_result(Result res);

    Result transmit(context::Context& ctx, void *ptr, uint32_t sz);

    virtual Result on_establish(context::Context& ctx) = 0;
    virtual Result on_shutdown(context::Context& ctx) = 0;
    virtual Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                              uint32_t& sent);
    virtual void on_delete(context::Context& ctx) {}

    Kind _kind = Kind::undefined; // must be properly set in the derived class ctor
    Connection *_link = nullptr;
    // std::atomic<bool> setting_link = false; // held in set_link()
    // std::atomic<bool> transmitting = false; // held in on_receive()

    std::mutex link_mx;

    struct {
        std::atomic<uint64_t> inbound_bytes;
        std::atomic<uint64_t> outbound_bytes;
        std::atomic<uint32_t> transactions_succeeded;
        std::atomic<uint32_t> transactions_failed;
        std::atomic<uint32_t> errors;

        int64_t  prev_timestamp_ms;
        uint64_t prev_inbound_bytes;
        uint64_t prev_outbound_bytes;
        uint32_t prev_errors;
        uint32_t prev_transactions_succeeded;
    } metrics;

private:
    virtual void collect(telemetry::Metric& metric, const int64_t& timestamp_ms);

    std::atomic<State> _state = State::not_configured;
    std::atomic<Status> _status = Status::initial;

    context::Context establish_ctx = context::WithCancel(context::Background());
    std::jthread establish_th;
    std::jthread shutdown_th;
};

const char * kind2str(Kind kind, bool brief = false);
const char * state2str(State state);
const char * status2str(Status status);
const char * result2str(Result res);

} // namespace mesh::connection

#endif // CONN_H
