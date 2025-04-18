/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_CLIENT_H
#define MESH_CLIENT_H

#include <list>
#include <mutex>
#include <string>
#include "mesh_dp.h"

namespace mesh {

class ConnectionContext;

class ClientConfig {
public:
    int parse_from_json(const char *str);

    std::string api_version;
    std::string proxy_ip;
    std::string proxy_port;
    int default_timeout_us;
    int max_conn_num;
};

/**
 * Mesh client context structure
 */
class ClientContext {
public:
    ClientContext();

    int init(const char *cfg);
    int create_conn(MeshConnection **conn, int kind);
    int shutdown();

    ClientConfig cfg;

    std::list<ConnectionContext *> conns;
    std::mutex mx;

    void *grpc_client = nullptr;
};

} // namespace mesh

/**
 * Max number of connections handled by mesh client by default
 */
#define MESH_CLIENT_DEFAULT_MAX_CONN 1024

/**
 * Default timeout applied to all mesh client operations
 */
#define MESH_CLIENT_DEFAULT_TIMEOUT_MS (MESH_TIMEOUT_INFINITE)

#endif // MESH_CLIENT_H
