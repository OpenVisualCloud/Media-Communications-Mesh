/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <thread>
#include <future>
#include <optional>
#include <queue>
#include <stdio.h>

#include <mutex>
#include <condition_variable>

namespace mesh {

namespace thread {

template<typename T>
class Channel;

} // namespace thread

namespace context {

/**
 * mesh::context::Context
 * 
 * Thread-safe context carrying a cancellation signal to be passed to threads
 * and blocking calls. Useful for graceful shutdown.
 */
class Context {
public:
    Context();
    virtual ~Context();
    Context& operator=(Context&& other) noexcept;

    void cancel();
    bool cancelled();
    std::stop_token stop_token();
    bool done();

    std::stop_source ss;

protected:
    Context(Context& parent);
    Context(Context& parent, std::chrono::milliseconds timeout_ms);

    Context *parent;
    thread::Channel<bool> *ch;
    std::future<void> async_cb;
    std::chrono::milliseconds timeout_ms;
    std::unique_ptr<std::stop_callback<std::function<void()>>> cb;

    friend Context& Background();
    friend class WithCancel;
    friend Context WithCancel(Context& parent);
    friend Context WithTimeout(Context& parent,
                               std::chrono::milliseconds timeout_ms);
};

/**
 * mesh::context::Background()
 * 
 * Returns an empty Context with cancellation. It is typically used in the
 * main function and initialization. It should be the parent context for
 * context::WithCancel and context::WithTimeout.
 */
Context& Background();

/**
 * mesh::context::WithCancel
 * 
 * Creates a new Context with cancellation. When the parent context is
 * cancelled, it triggers cancellation of the child context and all descending
 * child contexts in the chain. On the other hand, if the context is cancelled
 * by calling stop(), the parent context is not affected because the
 * cancellation signal propagates only down to the child contexts in the chain.
 * 
 * This concept enables propagation of the cancellation signal from the top
 * parent context down to the very bottom child context and stopping all
 * threads and blocking I/O calls that depend on any context in the chain.
 */
Context WithCancel(Context& parent);

/**
 * mesh::context::WithTimeout
 * 
 * Creates a new Context with cancellation and a timeout. The time interval
 * starts to count down at creation. When the time is out, the context is
 * cancelled, which triggers cancellation of all child contexts in the chain.
 */
Context WithTimeout(Context& parent, std::chrono::milliseconds timeout_ms);

} // namespace context

namespace thread {

/**
 * Defer
 * 
 * Allows to register a callback to be called when the defer object leaves
 * the scope of visibility. Deferred callbacks can be used for automatic
 * deallocation of resources created in a function when the function exits.
 * Example:
 * 
 * void func() {
 *     printf("Enter the function\n");
 *     ...
 *     Defer d1([]{ printf("Deferred action 1"); });
 *     ...
 *     Defer d2([]{ printf("Deferred action 2"); });
 *     ...
 *     printf("Exit the function\n");
 * }
 * 
 * Output:
 *     Enter the function
 *     Exit the function
 *     Deferred action 2
 *     Deferred action 1
 */
class Defer {
public:
    Defer(std::function<void()> callback) : cb(callback) {}
    ~Defer() { if (cb) cb(); }

private:
    std::function<void()> cb;
};

/**
 * Sleep
 * 
 * Sleeps for the given interval of time, which interrupts immediately when
 * the context is cancelled.
 */
void Sleep(context::Context& ctx, std::chrono::milliseconds interval_ms);

/**
 * Channel
 * 
 * A thread-safe queue template supporting cancellation of blocking calls
 * send() and receive() by checking context::Context. To set a timeout on the
 * blocking calls, context::WithTimeout is supposed to be used.
 * Example:
 * ...
 */
template<typename T>
class Channel {
public:
    Channel() : Channel(1) {}
    Channel(size_t capacity) : cap(capacity), _closed(false) {}

    bool send(context::Context& ctx, T value) {
        std::unique_lock<std::mutex> lk(mx);
        if (!cv_full.wait(lk, ctx.stop_token(), [this] { return !_closed && q.size() < cap; })) {
            return false;
        }
        q.push(std::move(value));
        cv_empty.notify_one();
        return true;
    }

    std::optional<T> receive(context::Context& ctx) {
        std::unique_lock<std::mutex> lk(mx);
        cv_empty.wait(lk, ctx.stop_token(), [this] { return !q.empty() || _closed; });
        if (ctx.stop_token().stop_requested()) {
            return std::nullopt;
        }
        if (q.empty()) {
            return std::nullopt;
        }
        T value = std::move(q.front());
        q.pop();
        cv_full.notify_one();
        return value;
    }

    std::optional<T> try_receive() {
        std::unique_lock<std::mutex> lk(mx);
        if (q.empty()) {
            return std::nullopt;
        }
        T value = std::move(q.front());
        q.pop();
        cv_full.notify_one();
        return value;
    }

    void close() {
        std::unique_lock<std::mutex> lk(mx);
        _closed = true;
        cv_empty.notify_all();
        cv_full.notify_all();
    }

    bool closed() {
        std::unique_lock<std::mutex> lk(mx);
        return _closed;
    }

private:
    std::queue<T> q;
    std::mutex mx;
    std::condition_variable_any cv_empty;
    std::condition_variable_any cv_full;
    size_t cap;
    bool _closed;
};

} // namespace thread

} // namespace mesh

#endif // CONCURRENCY_H
