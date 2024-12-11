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

namespace mesh::connection {

LocalManager local_manager;

// Temporary Multipoint group business logic.
// std::string tx_id, rx_id;

int LocalManager::create_connection_sdk(context::Context& ctx, std::string& id,
                                        mcm_conn_param *param,
                                        memif_conn_param *memif_param)
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

    const char *type_str = param->type == is_tx ? "tx" : "rx";

    snprintf(memif_ops.app_name, sizeof(memif_ops.app_name),
             "memif_%s_%s", type_str, id.c_str());
    snprintf(memif_ops.interface_name, sizeof(memif_ops.interface_name),
             "memif_%s_%s", type_str, id.c_str());
    snprintf(memif_ops.socket_path, sizeof(memif_ops.socket_path),
             "/run/mcm/media_proxy_%s_%s.sock", type_str, id.c_str());

    size_t frame_size = st_frame_size(ST_FRAME_FMT_YUV422PLANAR10LE,
                                      param->width, param->height, false);
    Local *conn;

    if (param->type == is_tx)
        conn = new(std::nothrow) LocalRx;
    else
        conn = new(std::nothrow) LocalTx;

    if (!conn)
        return -ENOMEM;

    auto res = conn->configure_memif(ctx, &memif_ops, frame_size);
    if (res != Result::success) {
        delete conn;
        return -1;
    }

    res = conn->establish(ctx);
    if (res != Result::success) {
        delete conn;
        return -1;
    }

    conn->get_params(memif_param);

    // Prepare parameters to register in Media Proxy
    std::string kind = param->type == is_tx ? "rx" : "tx";

    lock();
    thread::Defer d([this]{ unlock(); });

    // Register local connection in Media Proxy
    std::string agent_assigned_id;
    int err = proxyApiClient->RegisterConnection(agent_assigned_id, kind);
    if (err) {
        delete conn;
        return -1;
    }

    // Assign id accessed by metrics collector.
    conn->assign_id(agent_assigned_id);

    registry_sdk.replace(id, conn);
    registry.add(agent_assigned_id, conn);
    // log::debug("Added local conn")("conn_id", conn->id)("id", id);

    // // Temporary Multipoint group business logic.
    // // TODO: Remove when Multipoint Groups are implemented.
    // if (param->type == is_tx) {
    //     auto tx_conn = registry_sdk.get(tx_id);
    //     if (tx_conn) {
    //         conn->set_link(ctx, tx_conn);
    //         tx_conn->set_link(ctx, conn);
    //     }
    //     rx_id = id;
    // } else {
    //     auto rx_conn = registry_sdk.get(rx_id);
    //     if (rx_conn) {
    //         conn->set_link(ctx, rx_conn);
    //         rx_conn->set_link(ctx, conn);
    //     }
    //     tx_id = id;
    // }

    return 0;
}

int LocalManager::delete_connection_sdk(context::Context& ctx, const std::string& id)
{
    auto conn = registry_sdk.get(id);
    if (!conn)
        return -1;

    log::debug("Delete local conn")("conn_id", conn->id)("id", id);

    {
        lock();
        thread::Defer d([this]{ unlock(); });

        int err = proxyApiClient->UnregisterConnection(conn->id);
        if (err) {
            // TODO: Handle the error.
        }

        if (conn->link()) {
            // log::debug("Shutdown the link");
            conn->link()->set_link(ctx, nullptr, conn);
            conn->set_link(ctx, nullptr);
        }

        registry.remove(conn->id);
        registry_sdk.remove(id);
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

void LocalManager::shutdown(context::Context& ctx)
{
    auto ids = registry_sdk.get_all_ids();

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
