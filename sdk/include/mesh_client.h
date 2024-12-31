/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_CLIENT_H
#define MESH_CLIENT_H

#include <list>
#include <mutex>
#include "mesh_dp.h"

namespace mesh {

class ConnectionContext;

class ClientJsonConfig {
public:
    int parse_json(const char *str);

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
    ClientContext(MeshClientConfig *cfg);

    int init();
    int shutdown();
    int create_conn(MeshConnection **conn);

    int init_json(const char *cfg);
    int create_conn_json(MeshConnection **conn, int kind);

    MeshClientConfig config = {};

    bool enable_grpc_with_json = false;
    ClientJsonConfig cfg_json;

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

/**
 * Constants for marking uninitialized resources
 */
#define MESH_CONN_TYPE_UNINITIALIZED    -1 ///< Connection type is uninitialized
#define MESH_PAYLOAD_TYPE_UNINITIALIZED -1 ///< Payload type is uninitialized

#endif // MESH_CLIENT_H
