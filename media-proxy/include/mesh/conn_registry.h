/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_REGISTRY_H
#define CONN_REGISTRY_H

#include "conn.h"
#include <unordered_map>
#include <shared_mutex>
#include "uuid.h"

namespace mesh::connection {

/**
 * Registry
 * 
 * Thread-safe registry to store pointers to connection instances.
 */
class Registry {
public:
    int add(std::string& id, Connection *conn) {
        std::unique_lock lk(mx);
        if (conns.contains(id))
            return -1;
        conns[id] = conn;
        return 0;
    }

    void replace(const std::string& id, Connection *conn) {
        std::unique_lock lk(mx);
        conns[id] = conn;
    }

    bool remove(const std::string& id) {
        std::unique_lock lk(mx);
        return conns.erase(id) > 0;
    }

    Connection *get(const std::string& id) {
        std::shared_lock lk(mx);
        auto it = conns.find(id);
        if (it != conns.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::vector<std::string> get_all_ids() {
        std::shared_lock lk(mx);
        std::vector<std::string> ids;
        ids.reserve(conns.size());
        for (const auto& pair : conns)
            ids.push_back(pair.first);
        return ids;
    }

private:
    std::unordered_map<std::string, Connection *> conns;

    // TODO: Check if the mutex is still needed. The local connection manager
    // has it own mutex, which makes this one redundant.
    std::shared_mutex mx;
};

extern Registry registry;

} // namespace mesh::connection

#endif // CONN_REGISTRY_H
