/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MANAGER_MULTIPOINT_H
#define MANAGER_MULTIPOINT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <iostream>
#include "multipoint.h"
#include "manager_bridges.h"

namespace mesh::multipoint {

class GroupChangeConfig {
public:
    std::string group_id;
    connection::Config conn_config;
    std::vector<std::string> added_conn_ids;
    std::vector<std::string> deleted_conn_ids;
    std::vector<std::string> added_bridge_ids;
    std::vector<std::string> deleted_bridge_ids;
};

class GroupConfig {
public:
    connection::Config conn_config;
    std::vector<std::string> conn_ids;
    std::vector<std::string> bridge_ids;
};

class Config {
public:
    std::unordered_map<std::string, GroupConfig> groups;
    std::unordered_map<std::string, connection::BridgeConfig> bridges;
};

class GroupManager {
public:
    Result apply_config(context::Context& ctx, const Config& new_cfg);
    Result reconcile_config(context::Context& ctx,
                            std::vector<GroupChangeConfig> added_groups,
                            std::vector<GroupChangeConfig> deleted_groups,
                            std::vector<GroupChangeConfig> updated_groups);
    void unassociate_conn(const std::string& conn_id);
    void run(context::Context& ctx);

private:
    Group * create_group(const std::string& id, const std::string& engine);
    Result associate(context::Context& ctx, Group *group, Connection *conn);

    int register_group(const std::string& id, Group *group) {
        std::unique_lock lk(mx);
        if (groups.contains(id))
            return -1;
        groups[id] = group;
        return 0;
    }

    bool unregister_group(Group *group) {
        std::unique_lock lk(mx);
        deleted_groups[group->id] = group;
        return groups.erase(group->id) > 0;
    }

    Group * find_group(const std::string& id) {
        std::shared_lock lk(mx);
        auto it = groups.find(id);
        if (it != groups.end()) {
            return it->second;
        }
        return nullptr;
    }

    Config cfg;

    std::unordered_map<std::string, Group *> groups;
    std::unordered_map<std::string, Group *> deleted_groups;
    std::unordered_map<std::string, std::string> associations;
    std::shared_mutex mx;
};

extern GroupManager group_manager;

} // namespace mesh::multipoint

#endif // MANAGER_MULTIPOINT_H
