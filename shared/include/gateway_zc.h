/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GATEWAY_ZC_H
#define GATEWAY_ZC_H

#include "concurrency.h"

#ifdef SDK_BUILD
#include "mesh_logger.h"
#else // SDK_BUILD
#include "logger.h"
#endif // SDK_BUILD

namespace mesh::zerocopy {

class Config {
public:
    key_t sysv_key;
    size_t mem_region_sz;
};

} // namespace mesh::zerocopy

namespace mesh::zerocopy::gateway {

enum class State {
    not_configured,
    active,
    shutdown,
};

enum class Result {
    success,
    error_wrong_state,
    error_out_of_memory,
    error_general_failure,
    error_context_cancelled,
    error_config_invalid,
};

/**
 * Gateway
 */
class Gateway {
public:
    Gateway() {}
    virtual ~Gateway() {}

    State state();

    void set_state(State new_state);
    Result set_result(Result res);

    Result init(context::Context& ctx, const Config *cfg);
    Result shutdown(context::Context& ctx);

protected:
    virtual Result on_init(context::Context& ctx) = 0;
    virtual Result on_shutdown(context::Context& ctx) = 0;

    Config cfg;
    int shmid;
    void *mem_region_ptr = nullptr;
    void *payload = nullptr;
    uint32_t *seq = nullptr;

private:
    std::atomic<State> _state = State::not_configured;
};

const char * gw_state2str(State state);
const char * gw_result2str(Result res);

/**
 * GatewayTx
 */
class GatewayTx : public Gateway {
public:
    GatewayTx() : Gateway() {}
    ~GatewayTx() override {}

    Result set_tx_callback(std::function<Result(context::Context& ctx, void *ptr,
                                                uint32_t sz, uint32_t& sent)> cb);

private:
    Result on_init(context::Context& ctx) override;
    Result on_shutdown(context::Context& ctx) override;

    std::function<Result(context::Context& ctx, void *ptr, uint32_t sz,
                         uint32_t& sent)> tx_callback;
    std::jthread th;
    context::Context th_ctx = context::WithCancel(context::Background());
};

/**
 * GatewayRx
 */
class GatewayRx : public Gateway {
public:
    GatewayRx() : Gateway() {}

    Result allocate(void *& ptr, uint32_t sz);
    Result transmit(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent);

private:
    Result on_init(context::Context& ctx) override;
    Result on_shutdown(context::Context& ctx) override;
};

} // namespace mesh::zerocopy::gateway

#endif // GATEWAY_ZC_H
