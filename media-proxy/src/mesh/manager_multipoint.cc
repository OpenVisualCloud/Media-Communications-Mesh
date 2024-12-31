#include "manager_multipoint.h"
#include <unordered_set>
#include <utility>
#include <algorithm>
#include "logger.h"
#include "manager_local.h"
#include "manager_bridges.h"

namespace mesh::multipoint {

GroupManager group_manager;

Result GroupManager::apply_config(context::Context& ctx, const Config& new_cfg)
{
    std::unordered_set<std::string> current_group_ids;
    std::unordered_set<std::string> new_group_ids;

    for (const auto& [id, _] : cfg.groups)
        current_group_ids.insert(id);

    for (const auto& [id, _] : new_cfg.groups)
        new_group_ids.insert(id);

    std::vector<std::string> added_groups_ids;
    std::vector<std::string> deleted_groups_ids;
    std::vector<std::string> common_groups_ids;

    for (const auto& id : new_group_ids) {
        if (current_group_ids.find(id) == current_group_ids.end())
            added_groups_ids.emplace_back(id);
    }
    for (const auto& id : current_group_ids) {
        if (new_group_ids.find(id) == new_group_ids.end())
            deleted_groups_ids.emplace_back(id);
        else
            common_groups_ids.emplace_back(id);
    }

    std::vector<GroupChangeConfig> added_groups, deleted_groups, updated_groups;

    // Lambda function to find added and deleted connection ids in the group
    auto fn = [&](std::string group_id,
                  const std::vector<std::string>& current_ids,
                  const std::vector<std::string>& new_ids,
                  std::vector<std::string>& added_ids,
                  std::vector<std::string>& deleted_ids) {

        std::unordered_set<std::string> current_set(current_ids.begin(),
                                                    current_ids.end());
        std::unordered_set<std::string> new_set(new_ids.begin(),
                                                new_ids.end());

        for (const auto& id : new_ids) {
            if (current_set.find(id) == current_set.end())
                added_ids.emplace_back(id);
        }

        for (const auto& id : current_ids) {
            if (new_set.find(id) == new_set.end())
                deleted_ids.emplace_back(id);
        }
    };

    for (const auto& group_id : common_groups_ids) {
        auto it_current = cfg.groups.find(group_id);
        if (it_current == cfg.groups.end())
            continue;

        auto it_new = new_cfg.groups.find(group_id);
        if (it_new == new_cfg.groups.end())
            continue;

        std::vector<std::string> added_conn_ids;
        std::vector<std::string> deleted_conn_ids;

        fn(group_id, it_current->second.conn_ids, it_new->second.conn_ids,
           added_conn_ids, deleted_conn_ids);

        std::vector<std::string> added_bridge_ids;
        std::vector<std::string> deleted_bridge_ids;

        fn(group_id, it_current->second.bridge_ids, it_new->second.bridge_ids,
           added_bridge_ids, deleted_bridge_ids);

        if (added_conn_ids.empty() && deleted_conn_ids.empty() &&
            added_bridge_ids.empty() && deleted_bridge_ids.empty())
            continue;

        updated_groups.emplace_back(group_id, added_conn_ids, deleted_conn_ids,
                                    added_bridge_ids, deleted_bridge_ids);
    }

    for (const auto& group_id : added_groups_ids) {
        auto it = new_cfg.groups.find(group_id);
        if (it != new_cfg.groups.end())
            added_groups.emplace_back(group_id, it->second.conn_ids,
                                      std::vector<std::string>{},
                                      it->second.bridge_ids);
    }

    for (const auto& group_id : deleted_groups_ids) {
        auto it = cfg.groups.find(group_id);
        if (it != cfg.groups.end())
            deleted_groups.emplace_back(group_id, std::vector<std::string>{},
                                        it->second.conn_ids,
                                        std::vector<std::string>{},
                                        it->second.bridge_ids);
    }

    cfg = std::move(new_cfg);

    // // Print added_groups_ids
    // log::debug("----------------------------------------------");
    // for (const auto& group : added_groups) {
    //     log::debug("Added Group ID: %s", group.group_id.c_str());
    //     for (const auto& id : group.added_conn_ids) {
    //         log::debug("Added Conn ID: %s", id.c_str());
    //     }
    //     for (const auto& id : group.deleted_conn_ids) {
    //         log::debug("???Deleted Conn ID: %s", id.c_str());
    //     }
    // }

    // // Print deleted_groups_ids
    // log::debug("----------------------------------------------");
    // for (const auto& group : deleted_groups) {
    //     log::debug("Deleted Group ID: %s", group.group_id.c_str());
    //     for (const auto& id : group.added_conn_ids) {
    //         log::debug("???Added Conn ID: %s", id.c_str());
    //     }
    //     for (const auto& id : group.deleted_conn_ids) {
    //         log::debug("Deleted Conn ID: %s", id.c_str());
    //     }
    // }

    // // Print updated_groups
    // log::debug("----------------------------------------------");
    // for (const auto& group : updated_groups) {
    //     log::debug("Updated Group ID: %s", group.group_id.c_str());
    //     for (const auto& id : group.added_conn_ids) {
    //         log::debug("Added Conn ID: %s", id.c_str());
    //     }
    //     for (const auto& id : group.deleted_conn_ids) {
    //         log::debug("Deleted Conn ID: %s", id.c_str());
    //     }
    // }
    // log::debug("----------------------------------------------");

    if (ctx.cancelled())
        return Result::error_context_cancelled;

    return reconcile_config(ctx, added_groups, deleted_groups, updated_groups);
}

Result GroupManager::reconcile_config(context::Context& ctx,
                                      std::vector<GroupChangeConfig> added_groups,
                                      std::vector<GroupChangeConfig> deleted_groups,
                                      std::vector<GroupChangeConfig> updated_groups)
{
    if (added_groups.empty() && deleted_groups.empty() && updated_groups.empty()) {
        log::info("[RECONCILE] Config is up to date");
        return Result::success;
    }
    
    log::info("[RECONCILE] Started =========");

    local_manager.lock();
    thread::Defer d([]{ local_manager.unlock(); });

    // Delete entire groups, including associated connections and bridges
    for (const auto& cfg : deleted_groups) {
        Group *group = get_group(cfg.group_id);
        if (!group) {
            log::error("[RECONCILE] Delete group: not found")("group_id", cfg.group_id);
            continue;
        }

        log::info("[RECONCILE] Delete group and its conns")("group_id", cfg.group_id);

        if (group->link()) {
            group->link()->set_link(ctx, nullptr);
            group->set_link(ctx, nullptr);
        }

        group->shutdown(ctx);
        group->delete_all_outputs();

        for (const auto& bridge_id : cfg.deleted_bridge_ids) {
            auto err = bridges_manager.delete_bridge(ctx, bridge_id);
            if (err)
                log::error("[RECONCILE] Delete group del bridge: not found")
                          ("group_id", cfg.group_id)
                          ("bridge_id", bridge_id);
        }

        delete_group(cfg.group_id);
        delete group;
    }

    // Delete some connections and bridges in existing groups
    for (const auto& cfg : updated_groups) {
        Group *group = get_group(cfg.group_id);
        if (!group) {
            log::error("[RECONCILE] Update group del: not found")
                      ("group_id", cfg.group_id)
                      ("conns", cfg.deleted_conn_ids.size());
            continue;
        }

        for (const auto& conn_id : cfg.deleted_conn_ids) {
            auto conn = local_manager.get_connection(ctx, conn_id);
            if (!conn) {
                // log::error("[RECONCILE] Delete conn: not found")
                //           ("group_id", cfg.group_id)
                //           ("conn_id", conn_id);
                continue;
            }

            log::info("[RECONCILE] Delete conn")("group_id", cfg.group_id)
                                                ("conn_id", conn_id);

            if (conn->link()) {
                conn->link()->set_link(ctx, nullptr, conn);
                conn->set_link(ctx, nullptr);
            }
        }

        for (const auto& bridge_id : cfg.deleted_bridge_ids) {
            auto err = bridges_manager.delete_bridge(ctx, bridge_id);
            if (err)
                log::error("[RECONCILE] Update group del bridge: not found")
                          ("group_id", cfg.group_id)
                          ("bridge_id", bridge_id);
        }
    }

    // Lambda function for adding local connections to a group
    auto add_conns = [this, &ctx](Group *group, std::vector<std::string> conn_ids) {
        for (const auto& conn_id : conn_ids) {
            auto conn = local_manager.get_connection(ctx, conn_id);
            if (!conn) {
                log::error("[RECONCILE] Add conn: not found")
                          ("group_id", group->id)
                          ("conn_id", conn_id);
                continue;
            }

            log::info("[RECONCILE] Add conn")("group_id", group->id)
                                             ("conn_id", conn_id);

            auto res = associate(ctx, group, conn);
            if (res != Result::success)
                log::error("[RECONCILE] Add conn wrong kind")
                          ("group_id", group->id)
                          ("conn_id", conn_id);
        }
    };

    // Lambda function for adding bridges to a group
    auto add_bridges = [this, &ctx](Group *group,
                                    std::vector<std::string> bridge_ids) {
        for (const auto& bridge_id : bridge_ids) {
            Connection *bridge;
            Kind kind = Kind::transmitter;

            log::info("[RECONCILE] Add bridge")("group_id", group->id)
                                               ("bridge_id", bridge_id);

            auto it = cfg.bridges.find(bridge_id);
            if (it == cfg.bridges.end()) {
                log::error("[RECONCILE] Bridge cfg not found")
                          ("bridge_id", bridge_id);
                continue;
            }
            const auto& bridge_config = it->second;

            // DEBUG
            auto err = bridges_manager.create_bridge(ctx, bridge, bridge_id,
                                                     bridge_config);
            // DEBUG
            if (!bridge) {
                log::error("[RECONCILE] Add bridge err")
                          ("group_id", group->id)
                          ("bridge_id", bridge_id);
                          ("kind", kind2str(kind));
                continue;
            }

            auto res = associate(ctx, group, bridge);
            if (res != Result::success)
                log::error("[RECONCILE] Add bridge wrong kind")
                          ("group_id", group->id)
                          ("bridge_id", bridge_id);
        }
    };

    // Add new groups and their connections and bridges
    for (const auto& cfg : added_groups) {
        auto group = new(std::nothrow) Group(cfg.group_id);
        if (!group)
            return Result::error_out_of_memory;

        log::info("[RECONCILE] Add group")("group_id", group->id)
                                          ("conns", cfg.added_conn_ids.size())
                                          ("bridges", cfg.added_bridge_ids.size());

        group->configure(ctx);
        auto res = group->establish(ctx);
        if (res != Result::success) {
            log::error("[RECONCILE] Group establish err: %s", result2str(res))
                      ("group_id", group->id);
        }

        auto err = add_group(cfg.group_id, group);
        if (err) {
            log::error("[RECONCILE] Add group err: %d", err)("group_id", cfg.group_id);
            delete group;
            continue;
        }

        add_conns(group, cfg.added_conn_ids);
        add_bridges(group, cfg.added_bridge_ids);
    }

    // Add new connections to existing groups
    for (const auto& cfg : updated_groups) {
        Group *group = get_group(cfg.group_id);
        if (!group) {
            log::error("[RECONCILE] Update group: not found")("group_id", cfg.group_id);
            continue;
        }

        add_conns(group, cfg.added_conn_ids);
        add_bridges(group, cfg.added_bridge_ids);
    }

    log::info("[RECONCILE] Completed =======")("groups", groups.size());
    for (const auto& pair : groups) {
        Group* group = pair.second;

        log::info("* Group")
                 ("group_id", group->id)
                 ("input", group->link() ? "assigned" : "n/a")
                 ("outputs", group->outputs_num());
    }

    // TODO: Remove all bridges that are not associated to any group.

    return Result::success;
}

Result GroupManager::associate(context::Context& ctx, Group *group,
                               Connection *conn) {
    switch (conn->kind()) {
    case Kind::receiver:
        if (group->assign_input(ctx, conn) == Result::success)
            conn->set_link(ctx, group);
        return Result::success;

    case Kind::transmitter:
        if (conn->set_link(ctx, group) == Result::success)
            group->add_output(ctx, conn);
        return Result::success;

    default:
        return Result::error_bad_argument;
    }
};


} // namespace mesh::multipoint
