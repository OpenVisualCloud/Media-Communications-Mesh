/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "manager_local.h"
#include "conn_local_rx.h"
#include "conn_local_tx.h"
#include "conn_registry.h"
#include "uuid.h"
#include "logger.h"
#include <mtl/st_pipeline_api.h>
#include "proxy_api.h"
#include <cmath>

namespace mesh::connection {

LocalManager local_manager;

int LocalManager::create_connection_sdk(context::Context& ctx, std::string& id,
                                        const std::string& client_id,
                                        mcm_conn_param *param,
                                        memif_conn_param *memif_param,
                                        const Config& conn_config,
                                        std::string& err_str)
{
    if (!param)
        return -1;

    bool found = false;
    for (int i = 0; i < 5; i++) {
        id = generate_uuid_v4();
        int err = registry_sdk.add(id, nullptr);
        if (!err) {
            found = true;
            break;
        }
    }
    if (!found) {
        log::error("Registry contains UUID, max attempts.");
        return -1;
    }

    memif_ops_t memif_ops = {};
    memif_ops.interface_id = 0;

    const char *type_str = conn_config.kind2str();

    snprintf(memif_ops.app_name, sizeof(memif_ops.app_name),
             "memif_%s_%s", type_str, id.c_str());
    snprintf(memif_ops.interface_name, sizeof(memif_ops.interface_name),
             "memif_%s_%s", type_str, id.c_str());
    snprintf(memif_ops.socket_path, sizeof(memif_ops.socket_path),
             "/run/mcm/media_proxy_%s_%s.sock", type_str, id.c_str());

    size_t frame_size = conn_config.buf_parts.total_size();

    Local *conn;

    if (conn_config.kind == sdk::CONN_KIND_TRANSMITTER)
        conn = new(std::nothrow) LocalRx;
    else
        conn = new(std::nothrow) LocalTx;

    if (!conn) {
        registry_sdk.remove(id);
        return -ENOMEM;
    }

    conn->set_config(conn_config);
    conn->set_parent(client_id);

    uint8_t log2_ring_size = conn_config.buf_queue_capacity ?
                             std::log2(conn_config.buf_queue_capacity) : 0;
    auto res = conn->configure_memif(ctx, &memif_ops, frame_size, log2_ring_size);
    if (res != Result::success) {
        registry_sdk.remove(id);
        delete conn;
        return -1;
    }

    // Prepare parameters to register in Media Proxy
    std::string kind = conn_config.kind == sdk::CONN_KIND_TRANSMITTER ? "rx" : "tx";

    lock();
    thread::Defer d([this]{ unlock(); });

    // Register local connection in Media Proxy
    std::string agent_assigned_id;
    int err = proxyApiClient->RegisterConnection(agent_assigned_id, kind,
                                                 conn_config, err_str);
    if (err) {
        registry_sdk.remove(id);
        delete conn;
        return -1;
    }

    res = conn->establish(ctx);
    if (res != Result::success) {
        registry_sdk.remove(id);
        delete conn;
        return -1;
    }

    conn->get_params(memif_param);

    // Assign id accessed by metrics collector.
    conn->assign_id(agent_assigned_id);

    // TODO: Rethink the flow to avoid using two registries with different ids.
    registry_sdk.replace(id, conn);
    registry.add(agent_assigned_id, conn);

    conn->legacy_sdk_id = id; // Remember the id generated in Media Proxy.
    id = agent_assigned_id;   // Let SDK use the Agent provided id.

    return 0;
}

Result LocalManager::activate_connection_sdk(context::Context& ctx, const std::string& id)
{
    auto conn = registry.get(id);
    if (!conn)
        return Result::error_bad_argument;

    if (!conn->link())
        return Result::error_no_link_assigned;

    log::debug("Activate local conn")("conn_id", conn->id)("id", id);

    {
        lock(); // TODO: Check if locking is needed for conn activation.
        thread::Defer d([this]{ unlock(); });

        conn->resume(ctx);
    }

    return Result::success;
}

int LocalManager::delete_connection_sdk(context::Context& ctx, const std::string& id,
                                        bool do_unregister)
{
    auto conn = registry.get(id);
    if (!conn)
        return -1;

    log::debug("Delete local conn")("conn_id", conn->id)("id", id);

    {
        lock();
        thread::Defer d([this]{ unlock(); });

        if (do_unregister) {
            int err = proxyApiClient->UnregisterConnection(conn->id);
            if (err) {
                // TODO: Handle the error.
            }
        }

        if (conn->link()) {
            // log::debug("Shutdown the link");
            conn->link()->set_link(ctx, nullptr, conn);
            conn->set_link(ctx, nullptr);
        }

        registry.remove(conn->id);
        registry_sdk.remove(conn->legacy_sdk_id);
    }

    auto res = conn->shutdown(ctx);
    delete conn;

    return 0;
}

Connection * LocalManager::get_connection(context::Context& ctx,
                                          const std::string& id)
{
    return registry.get(id);
}

int LocalManager::reregister_all_connections(context::Context& ctx)
{
    auto ids = registry.get_all_ids();

    Config conn_config;

    log::debug("Re-register all conns");
    for (std::string id : ids) {
        auto conn = registry.get(id);
        if (!conn)
            continue;

        log::debug("Re-register conn")
                  ("conn_id", conn->id);

        std::string kind = conn->config.kind == sdk::CONN_KIND_TRANSMITTER ? "rx" : "tx";

        std::string err_unused;
        std::string existing_conn_id = conn->id;

        int err = proxyApiClient->RegisterConnection(existing_conn_id, kind,
                                                     conn->config, err_unused);
        if (err) {
            log::error("Error re-registering local conn (%d)", err)
                      ("conn_id", conn->id);

            err = delete_connection_sdk(ctx, id, false);
            if (err)
                log::error("Re-register: error deleting local conn (%d)", err)
                          ("conn_id", conn->id);
        }
    }

    return 0;
}

int LocalManager::notify_all_shutdown_wait(context::Context& ctx)
{
    auto ids = registry.get_all_ids();

    lock();

    for (const std::string& id : ids) {
        auto conn = registry.get(id);
        if (conn)
            conn->notify_parent_conn_unlink_requested(ctx);
    }

    unlock();

    while (!ctx.cancelled()) {
        if (registry.size() == 0)
            return 0;

        thread::Sleep(ctx, std::chrono::milliseconds(100));
    }

    return -1;
}

void LocalManager::shutdown(context::Context& ctx)
{
    auto err = notify_all_shutdown_wait(ctx);
    if (err)
        log::error("Shutdown notification timeout");

    auto ids = registry.get_all_ids();

    for (const std::string& id : ids) {
        auto err = delete_connection_sdk(ctx, id);
        if (err)
            log::error("Error deleting local conn (%d)", err)
                      ("conn_id", id);
    }
}

void LocalManager::lock()
{
    // log::debug("Local Manager mx lock");
    mx.lock();
}

void LocalManager::unlock()
{
    // log::debug("Local Manager mx unlock");
    mx.unlock();
}

} // namespace mesh::connection
