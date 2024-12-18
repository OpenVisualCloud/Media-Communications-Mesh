#ifndef MOCKED_BRIDGE_H
#define MOCKED_BRIDGE_H

#include "conn.h"

namespace mesh::connection {

class MockedBridge : public Connection {
public:
    MockedBridge() {}

    void configure(context::Context& ctx, Kind kind) {
        _kind = kind;
        set_state(ctx, State::configured);
    }

    Result on_establish(context::Context& ctx) override {
        set_state(ctx, State::active);
        return Result::success;
    }

    Result on_shutdown(context::Context& ctx) override {
        set_state(ctx, State::closed);
        return Result::success;
    }

    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                      uint32_t& sent) override {
        sent = sz;
        return Result::success;
    }
};

} // namespace mesh::connection

#endif // MOCKED_BRIDGE_H
