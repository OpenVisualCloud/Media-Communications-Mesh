/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "gateway_zc.h"
#include <sys/shm.h>
#include <cstring>

namespace mesh::zerocopy::gateway {

Result Gateway::init(context::Context& ctx, const Config *cfg)
{
    switch (state()) {
    case State::not_configured:
    case State::shutdown:
        break;
    default:
        return set_result(Result::error_wrong_state);
    }

    if (!cfg)
        return set_result(Result::error_config_invalid);

    this->cfg = *cfg;

    auto res = on_init(ctx);
    if (res == Result::success)
        set_state(State::active);
    else
        set_state(State::not_configured);

    return res;
}

Result Gateway::shutdown(context::Context& ctx)
{
    if (state() != State::active)
        return set_result(Result::error_wrong_state);

    auto res = on_shutdown(ctx);
    set_state(State::shutdown);

    return res;
}

State Gateway::state()
{
    return _state.load(std::memory_order_acquire);
}

void Gateway::set_state(State new_state)
{
    _state.exchange(new_state, std::memory_order_release);
}

Result Gateway::set_result(Result res)
{
    return res;
}

static const char str_unknown[] = "?unknown?";

const char * gw_state2str(State state)
{
    switch (state) {
    case State::not_configured: return "not configured";
    case State::active:         return "active";
    case State::shutdown:       return "shutdown";
    default:                    return str_unknown;
    }
}

const char * gw_result2str(Result res)
{
    switch (res) {
    case Result::success:                 return "success";
    case Result::error_wrong_state:       return "wrong state";
    case Result::error_out_of_memory:     return "out of memory";
    case Result::error_general_failure:   return "general failure";
    case Result::error_context_cancelled: return "context cancelled";
    case Result::error_config_invalid:    return "invalid config";
    default:                              return str_unknown;
    }
}

/**
 * GatewayTx
 */

Result GatewayTx::set_tx_callback(std::function<Result(context::Context& ctx,
                                                       void *ptr, uint32_t sz,
                                                       uint32_t& sent)> cb)
{
    tx_callback = cb;
    return set_result(Result::success);
}

Result GatewayTx::on_init(context::Context& ctx)
{
    shmid = shmget(cfg.sysv_key, cfg.mem_region_sz, 0666);
    if (shmid < 0)
        return Result::error_config_invalid;

    mem_region_ptr = shmat(shmid, nullptr, 0);
    if (mem_region_ptr == (void *)(-1))
        return Result::error_general_failure;

    payload = (uint32_t *)mem_region_ptr + 1;
    seq = (uint32_t *)mem_region_ptr;

    try {
        th_ctx = context::WithCancel(ctx);
        th = std::jthread([&]() {
            uint32_t prev = *seq;

            while (!th_ctx.cancelled()) {
                auto cur = *seq;
                if (prev != cur) {
                    prev = cur;

                    if (tx_callback) {
                        uint32_t sent = 0;
                        tx_callback(th_ctx, payload, cfg.mem_region_sz, sent);
                    }
                }
                thread::Sleep(th_ctx, std::chrono::milliseconds(5));

                // if (tx_callback) {
                //     uint32_t sent = 0;
                //     char str[] = "1234567890";
                //     tx_callback(th_ctx, str, sizeof(str), sent);
                //     thread::Sleep(th_ctx, std::chrono::milliseconds(100));
                // }
            }

            log::debug("EXIT gw tx thread");
        });
    }
    catch (const std::system_error& e) {
        log::error("thread create failed");
        return set_result(Result::error_out_of_memory);
    }

    return Result::success;
}

Result GatewayTx::on_shutdown(context::Context& ctx)
{
    th_ctx.cancel();
    th.join();

    shmdt(mem_region_ptr);
    return Result::success;
}

/**
 * GatewayRx
 */

Result GatewayRx::on_init(context::Context& ctx)
{
    shmid = shmget(cfg.sysv_key, cfg.mem_region_sz, 0666);
    if (shmid < 0)
        return Result::error_config_invalid;

    mem_region_ptr = shmat(shmid, nullptr, 0);
    if (mem_region_ptr == (void *)(-1))
        return Result::error_general_failure;

    payload = (uint32_t *)mem_region_ptr + 1;
    seq = (uint32_t *)mem_region_ptr;
    *seq = 0;

    return Result::success;
}

Result GatewayRx::on_shutdown(context::Context& ctx)
{
    shmdt(mem_region_ptr);
    return Result::success;
}

 Result GatewayRx::allocate(void *& ptr, uint32_t sz)
{
    ptr = NULL;
    return set_result(Result::success);
}

Result GatewayRx::transmit(context::Context& ctx, void *ptr, uint32_t sz,
                           uint32_t& sent)
{
    if (state() != State::active)
        return Result::error_wrong_state;

    // log::debug("GatewayRx transmit %u", sz);

    *seq += 1;
    std::memcpy(payload, ptr, sz);

    sent = sz;

    return set_result(Result::success);
}

} // namespace mesh::zerocopy::gateway
