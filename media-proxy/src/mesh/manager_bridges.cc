/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "manager_bridges.h"
#include "conn_registry.h"
#include "logger.h"
#include "mocked_bridge.h"

namespace mesh::connection {

BridgesManager bridges_manager;

int BridgesManager::create_bridge(context::Context& ctx, Connection*& bridge,
                                  const std::string& id, Kind kind)
{
    // DEBUG
    auto mocked_bridge = new(std::nothrow) MockedBridge;
    if (!mocked_bridge)
        return -ENOMEM;
    // DEBUG

    // if (param->type == is_tx)
    //     conn = new(std::nothrow) LocalRx;
    // else
    //     conn = new(std::nothrow) LocalTx;

    // if (!conn)
    //     return -ENOMEM;

    // auto res = bridge->configure_memif(ctx, &memif_ops, frame_size);
    // if (res != Result::success) {
    //     delete conn;
    //     return -1;
    // }

    mocked_bridge->configure(ctx, kind);

    bridge = mocked_bridge;

    auto res = bridge->establish(ctx);
    if (res != Result::success) {
        delete bridge;
        return -1;
    }

    lock();
    thread::Defer d([this]{ unlock(); });

    // Assign id accessed by metrics collector.
    bridge->assign_id(id);

    registry.add(id, bridge);

    return 0;
}

int BridgesManager::delete_bridge(context::Context& ctx, const std::string& id)
{
    auto bridge = registry.get(id);
    if (!bridge)
        return -1;

    {
        lock();
        thread::Defer d([this]{ unlock(); });

        if (bridge->link()) {
            // log::debug("Shutdown the link");
            bridge->link()->set_link(ctx, nullptr, bridge);
            bridge->set_link(ctx, nullptr);
        }

        registry.remove(id);
    }

    auto res = bridge->shutdown(ctx);
    delete bridge;

    return 0;
}

Connection * BridgesManager::get_bridge(context::Context& ctx,
                                        const std::string& id)
{
    return registry.get(id);
}

void BridgesManager::shutdown(context::Context& ctx)
{
    auto ids = registry.get_all_ids();

    for (const std::string& id : ids) {
        auto err = delete_bridge(ctx, id);
        if (err)
            log::error("Error deleting bridge (%d)", err)
                      ("bridge_id", id);
    }
}

void BridgesManager::lock()
{
    // log::debug("Bridges Manager mx lock");
    mx.lock();
}

void BridgesManager::unlock()
{
    // log::debug("Bridges Manager mx unlock");
    mx.unlock();
}

} // namespace mesh::connection
