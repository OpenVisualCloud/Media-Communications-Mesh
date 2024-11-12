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

/**
 * Mesh client context structure
 */
class ClientContext {
public:
    ClientContext(MeshClientConfig *cfg);
    int shutdown();
    int create_conn(MeshConnection **conn);

    MeshClientConfig config = {};

    std::list<ConnectionContext *> conns;
    std::mutex mx;
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
