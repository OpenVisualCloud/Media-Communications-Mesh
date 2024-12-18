/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MANAGER_BRIDGES_H
#define MANAGER_BRIDGES_H

#include <string>
#include "concurrency.h"
#include "conn_registry.h"

namespace mesh::connection {

class BridgeConfig {
public:
    std::string type;
    Kind kind;

    struct {
        std::string remote_ip;
        uint16_t port;
        std::string transport;
    } st2110;

    struct {
        std::string remote_ip;
        uint16_t port;
    } rdma;
};

class BridgesManager {
public:
    int create_bridge(context::Context& ctx, Connection*& bridge,
                      const std::string& id, const BridgeConfig& cfg);

    int delete_bridge(context::Context& ctx, const std::string& id);

    Connection * get_bridge(context::Context& ctx, const std::string& id);

    void shutdown(context::Context& ctx);

    void lock();
    void unlock();

private:
    Registry registry; // This regustry uses Agent assigned ids
    std::shared_mutex mx;
};

extern BridgesManager bridges_manager;

} // namespace mesh::connection

#endif // MANAGER_BRIDGES_H
