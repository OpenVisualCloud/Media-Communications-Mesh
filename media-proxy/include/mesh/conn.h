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
    error_shutdown,
    // TODO: more error codes to be added...
};

/**
 * Connection
 * 
 * Base abstract class of connection. All connection implementations must
 * inherit this class.
 */
class Connection {

public:
    Connection();
    virtual ~Connection();

    Kind kind();
    State state();
    Status status();

    Result set_link(context::Context& ctx, Connection *new_link);
    Connection * link();

    Result establish(context::Context& ctx);
    Result suspend(context::Context& ctx);
    Result resume(context::Context& ctx);
    Result shutdown(context::Context& ctx);

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

    Kind _kind; // must be properly set in the derived classes ctor
    Connection *_link;

    struct {
        std::atomic<uint64_t> inbound_bytes;
        std::atomic<uint64_t> outbound_bytes;
        std::atomic<uint32_t> transactions_successful;
        std::atomic<uint32_t> transactions_failed;
        std::atomic<uint32_t> errors;
    } metrics;

private:
    std::atomic<State> _state;
    std::atomic<Status> _status;
    std::atomic<bool> setting_link; // held in set_link()
    std::atomic<bool> transmitting; // held in on_receive()
};

const char * kind2str(Kind kind, bool brief = false);
const char * state2str(State state);
const char * status2str(Status status);
const char * result2str(Result res);

} // namespace mesh::connection

#endif // CONN_H
