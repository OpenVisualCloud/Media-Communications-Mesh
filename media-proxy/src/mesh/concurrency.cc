/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "concurrency.h"
#include <stdio.h>

namespace mesh {

namespace context {

Context::Context() : ss(std::stop_source()), ch(new thread::Channel<bool>(1))
{
}

Context::~Context()
{
    cancel();
    delete ch;
}

void Context::cancel() 
{
    ch->close();
    ss.request_stop();
}

bool Context::cancelled()
{
    return ss.stop_requested();
}

std::stop_token Context::stop_token()
{
    return ss.get_token();
}

bool Context::done()
{
    ch->receive(*this);
    return true;
}

Context& Background()
{
    static Context backgroundContext;
    return backgroundContext;
}

WithCancel::WithCancel(Context& parent) : Context(), parent(parent)
{
    std::stop_callback(parent.ss.get_token(), [this] { cancel(); });
}

WithTimeout::WithTimeout(Context& parent,
                         std::chrono::milliseconds timeout_ms) : WithCancel(parent)
{
    async_cb = std::async(std::launch::async, [this, timeout_ms] {
        std::mutex mx;
        std::unique_lock<std::mutex> lk(mx);
        std::condition_variable_any cv;
        cv.wait_for(lk, ss.get_token(), timeout_ms, [] { return false; });
        cancel();
    });
}

WithTimeout::~WithTimeout()
{
    cancel();
}

} // namespace context

namespace thread {

void Sleep(context::Context& ctx, std::chrono::milliseconds interval_ms)
{
    std::mutex mx;
    std::unique_lock<std::mutex> lk(mx);
    std::condition_variable_any cv;
    cv.wait_for(lk, ctx.stop_token(), interval_ms, [] { return false; });
}

} // namespace thread

} // namespace mesh

