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
#include "conn_local_zc_wrap_rx.h"
#include "conn_local_zc_wrap_tx.h"

namespace mesh::connection {

LocalManager local_manager;

Result LocalManager::make_connection(context::Context& ctx, const Config& cfg,
                                     Connection *& conn, Local *& memif_conn)
{
    if (!cfg.options.engine.compare("zero-copy")) {
        if (cfg.kind == sdk::CONN_KIND_TRANSMITTER) {
            auto zc_conn = new(std::nothrow) ZeroCopyWrapperLocalRx;
            conn = zc_conn;
            if (conn) {
                memif_conn = zc_conn->get_memif_conn();
                zc_conn->configure(ctx);
            }
        } else {
            memif_conn = new(std::nothrow) ZeroCopyWrapperLocalTx;
            conn = memif_conn;
        }
    } else {
        if (cfg.kind == sdk::CONN_KIND_TRANSMITTER) {
            memif_conn = new(std::nothrow) LocalRx;
            conn = memif_conn;
        } else {
            memif_conn = new(std::nothrow) LocalTx;
            conn = memif_conn;
        }
    }

    if (!conn || !memif_conn) {
        delete conn;
        delete memif_conn;
        return Result::error_out_of_memory;
    }

    return Result::success;
}

int LocalManager::create_connection_sdk(context::Context& ctx, std::string& id,
                                        const std::string& client_id,
                                        mcm_conn_param *param,
                                        memif_conn_param *memif_param,
                                        const Config& conn_config,
                                        const std::string& name,
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

    Connection *conn = nullptr;
    Local *memif_conn = nullptr;

    auto res = make_connection(ctx, conn_config, conn, memif_conn);
    if (res != Result::success) {
        registry_sdk.remove(id);
        return -ENOMEM;
    }

    conn->set_config(conn_config);
    memif_conn->set_config(conn_config);

    conn->log_dump_config();

    conn->set_parent(client_id);
    conn->set_name(name);

    res = memif_conn->configure_memif(ctx);
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
                                                 conn_config, name, err_str);
    if (err) {
        registry_sdk.remove(id);
        delete conn;
        return -1;
    }

    res = conn->establish(ctx);
    if (res != Result::success) {
        log::error("Local conn establish failed: %s", result2str(res))
                  ("conn_id", agent_assigned_id);
        registry_sdk.remove(id);
        delete conn;
        return -1;
    }

    memif_conn->get_params_memif(memif_param);

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

Connection * LocalManager::find_connection(context::Context& ctx,
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
                                                     conn->config, conn->name(),
                                                     err_unused);
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
