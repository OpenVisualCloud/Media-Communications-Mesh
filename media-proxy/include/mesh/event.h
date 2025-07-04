/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <unordered_map>
#include <mutex>
#include "concurrency.h"
#include "logger.h"
#include <unordered_map>
#include <any>

namespace mesh::event {

enum class Type {
    empty_event,
    conn_unlink_requested,
};

typedef void (* Handler)(const Type& type);

class Event {
public:
    std::string consumer_id;
    Type type;
    std::unordered_map<std::string, std::any> params;
};

/**
 * EventBroker
 * 
 * Subsystem for passing events from producers to consumers.
 * Enables software component decoupling.
 */
class EventBroker {
public:
    EventBroker() {}

    thread::Channel<Event> * subscribe(const std::string& consumer_id, int queue_sz = 1) {
        auto ch = new(std::nothrow) thread::Channel<Event>(queue_sz);
        if (ch) {
            std::unique_lock lk(mx);
            channels[ch] = consumer_id;
        }
        return ch;
    }

    bool unsubscribe(thread::Channel<Event> *ch) {
        std::unique_lock lk(mx);
        auto ret = channels.erase(ch) > 0;
        delete ch;
        return ret;
    }

    bool send(context::Context& ctx, const std::string& consumer_id, const Type type,
              const std::unordered_map<std::string, std::any>& params = {}) {
        Event evt = {
            .consumer_id = consumer_id,
            .type = type,
            .params = params,
        };
        return events.send(ctx, evt);
    }

    void run(context::Context& ctx) {
        for (;;) {
            auto v = events.receive(ctx);
            if (ctx.cancelled())
                return;

            if (!v.has_value())
                continue;

            auto evt = v.value();
            for (const auto& [ch, id] : channels) {
                if (id == evt.consumer_id) {
                    auto tctx = context::WithTimeout(ctx, std::chrono::milliseconds(3000));
                    if (ctx.cancelled())
                        return;

                    if (tctx.cancelled()) {
                        log::error("Event sending timeout")("type", (int)evt.type)
                                  ("consumer_id", evt.consumer_id);
                    } else if (!ch->send(tctx, evt)) {
                        log::error("Event sending failed")("type", (int)evt.type)
                                  ("consumer_id", evt.consumer_id);
                    }
                }
            }
        }
    }

private:
    std::unordered_map<thread::Channel<Event> *, std::string> channels;
    std::mutex mx;
    thread::Channel<Event> events = thread::Channel<Event>(100);
};

extern EventBroker broker;

} // namespace mesh::event

#endif // EVENT_H
