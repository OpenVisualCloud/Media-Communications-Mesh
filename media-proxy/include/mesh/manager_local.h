/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MANAGER_LOCAL_H
#define MANAGER_LOCAL_H

#include "mcm_dp.h"
#include <string>
#include "concurrency.h"
#include "conn_registry.h"

namespace mesh::connection {

class LocalManager {
public:
    int create_connection_sdk(context::Context& ctx, std::string& id,
                              const std::string& client_id, mcm_conn_param *param,
                              memif_conn_param *memif_param,
                              const Config& conn_config, std::string& err_str);

    Result activate_connection_sdk(context::Context& ctx, const std::string& id);

    int delete_connection_sdk(context::Context& ctx, const std::string& id,
                              bool do_unregister = true);

    Connection * get_connection(context::Context& ctx, const std::string& id);

    int reregister_all_connections(context::Context& ctx);

    int notify_all_shutdown_wait(context::Context& ctx);

    void shutdown(context::Context& ctx);

    void lock();
    void unlock();

private:
    Registry registry_sdk; // This registry uses SDK ids
    Registry registry;     // This registry uses Agent assigned ids
    std::mutex mx;
};

extern LocalManager local_manager;

} // namespace mesh::connection

#endif // MANAGER_LOCAL_H
