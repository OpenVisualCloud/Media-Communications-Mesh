/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "concurrency.h"
#include <stdio.h>

namespace mesh {

namespace context {

Context::Context() : ss(std::stop_source()),
                     parent(nullptr),
                     cb(nullptr),
                     ch(new thread::Channel<bool>(1)),
                     timeout_ms(std::chrono::milliseconds(0))
{
}

Context::Context(Context& parent) : ss(std::stop_source()),
                                    parent(&parent),
                                    ch(new thread::Channel<bool>(1))
{
    cb = std::make_unique<std::stop_callback<std::function<void()>>>(
        parent.ss.get_token(), [this] { cancel(); }
    );
}

Context::Context(Context& parent,
                 std::chrono::milliseconds timeout_ms) : ss(std::stop_source()),
                                                         parent(&parent),
                                                         ch(new thread::Channel<bool>(1)),
                                                         timeout_ms(timeout_ms)
{
    cb = std::make_unique<std::stop_callback<std::function<void()>>>(
        parent.ss.get_token(), [this] { cancel(); }
    );

    if (timeout_ms != std::chrono::milliseconds(0)) {
        async_cb = std::async(std::launch::async, [this] {
            std::mutex mx;
            std::unique_lock<std::mutex> lk(mx);
            std::condition_variable_any cv;
            cv.wait_for(lk, ss.get_token(), this->timeout_ms, [] { return false; });
            cancel();
        });
    }
}

Context& Context::operator=(Context&& other) noexcept {
    if (this != &other) {
        ss = std::stop_source();
        parent = other.parent;

        cb = std::make_unique<std::stop_callback<std::function<void()>>>(
            parent->ss.get_token(), [this] { cancel(); }
        );

        timeout_ms = other.timeout_ms;

        if (timeout_ms != std::chrono::milliseconds(0)) {
            async_cb = std::async(std::launch::async, [this] {
                std::mutex mx;
                std::unique_lock<std::mutex> lk(mx);
                std::condition_variable_any cv;
                cv.wait_for(lk, ss.get_token(), timeout_ms, [] { return false; });
                cancel();
            });
        }

        ch = other.ch;
        other.ch = nullptr;
    }
    return *this;
}

Context::~Context()
{
    cancel();

    if (async_cb.valid())
        async_cb.wait();

    if (ch)
        delete ch;
}

void Context::cancel() 
{
    if (ch)
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
    if (ch)
        ch->receive(*this);

    return true;
}

Context& Background()
{
    static Context backgroundContext;
    return backgroundContext;
}

Context WithCancel(Context& parent)
{
    return Context(parent);
}

Context WithTimeout(Context& parent, std::chrono::milliseconds timeout_ms)
{
    return Context(parent, timeout_ms);
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

