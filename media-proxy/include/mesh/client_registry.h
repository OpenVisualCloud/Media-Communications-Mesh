/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CLIENT_REGISTRY_H
#define CLIENT_REGISTRY_H

#include "client.h"
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include "uuid.h"

namespace mesh::client {

/**
 * Registry
 * 
 * Thread-safe registry to store pointers to SDK client instances.
 */
class Registry {
public:
    int add(const std::string& id, Client *client) {
        std::unique_lock lk(mx);
        if (clients.contains(id))
            return -1;
        clients[id] = client;
        return 0;
    }

    bool remove(const std::string& id) {
        std::unique_lock lk(mx);
        return clients.erase(id) > 0;
    }

    Client * get(const std::string& id) {
        std::shared_lock lk(mx);
        auto it = clients.find(id);
        if (it != clients.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, Client *> clients;
    std::shared_mutex mx;
};

extern Registry registry;

} // namespace mesh::client

#endif // CLIENT_REGISTRY_H
