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

namespace mesh::multipoint {

class GroupChangeConfig {
public:
    std::string group_id;
    std::vector<std::string> added_conn_ids;
    std::vector<std::string> deleted_conn_ids;
    std::vector<std::string> added_bridge_ids;
    std::vector<std::string> deleted_bridge_ids;
};

class GroupConfig {
public:
    std::vector<std::string> conn_ids;
    std::vector<std::string> bridge_ids;
};

class Config {
public:
    std::unordered_map<std::string, GroupConfig> groups;
};

class GroupManager {
public:
    Result apply_config(context::Context& ctx, const Config& new_cfg);
    Result reconcile_config(context::Context& ctx,
                            std::vector<GroupChangeConfig> added_groups,
                            std::vector<GroupChangeConfig> deleted_groups,
                            std::vector<GroupChangeConfig> updated_groups);
private:
    Result associate(context::Context& ctx, Group *group, Connection *conn);

    int add_group(const std::string& id, Group *group) {
        std::unique_lock lk(mx);
        if (groups.contains(id))
            return -1;
        groups[id] = group;
        return 0;
    }

    bool delete_group(const std::string& id) {
        std::unique_lock lk(mx);
        return groups.erase(id) > 0;
    }

    Group * get_group(const std::string& id) {
        std::shared_lock lk(mx);
        auto it = groups.find(id);
        if (it != groups.end()) {
            return it->second;
        }
        return nullptr;
    }

    Config cfg;

    std::unordered_map<std::string, Group *> groups;
    std::shared_mutex mx;
};

extern GroupManager group_manager;

} // namespace mesh::multipoint

#endif // MANAGER_MULTIPOINT_H
