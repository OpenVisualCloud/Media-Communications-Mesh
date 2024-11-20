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
#include <mtl/st30_pipeline_api.h>

namespace mesh::connection {

LocalManager local_manager;

std::string tx_id, rx_id;

int LocalManager::create_connection(context::Context& ctx, std::string& id,
                                    mcm_conn_param *param,
                                    memif_conn_param *memif_param)
{
    if (!param)
        return -1;

    bool found = false;
    for (int i = 0; i < 5; i++) {
        id = generate_uuid_v4();
        int err = registry.add(id, nullptr);
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

    std::unique_lock lk(mx);

    registry.replace(id, conn);

    // Temporary Multipoint group business logic.
    // TODO: Remove when Multipoint Groups are implemented.
    if (param->type == is_tx) {
        auto tx_conn = registry.get(tx_id);
        if (tx_conn) {
            conn->set_link(ctx, tx_conn);
            tx_conn->set_link(ctx, conn);
        }
        rx_id = id;
    } else {
        auto rx_conn = registry.get(rx_id);
        if (rx_conn) {
            conn->set_link(ctx, rx_conn);
            rx_conn->set_link(ctx, conn);
        }
        tx_id = id;
    }

    return 0;
}

int LocalManager::delete_connection(context::Context& ctx, const std::string& id)
{
    std::unique_lock lk(mx);

    auto conn = registry.get(id);
    if (!conn)
        return -1;

    auto link = conn->link();
    if (link) {
        log::debug("Shutdown the link");
        link->set_link(ctx, nullptr);
        conn->set_link(ctx, nullptr);
    }

    registry.remove(id);

    auto res = conn->shutdown(ctx);
    delete conn;

    return 0;
}

void LocalManager::shutdown(context::Context& ctx)
{
    auto ids = registry.get_all_ids();

    for (const std::string& id : ids) {
        auto err = this->delete_connection(ctx, id);
        if (err)
            log::error("Error deleting local conn (%d)", err);
    }
}

} // namespace mesh::connection
