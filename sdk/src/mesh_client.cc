/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_client.h"
#include <string.h>
#include "mesh_conn.h"

namespace mesh {

ClientContext::ClientContext(MeshClientConfig *cfg)
{
    if (cfg)
        memcpy(&config, cfg, sizeof(MeshClientConfig));

    if (config.max_conn_num == 0)
        config.max_conn_num = MESH_CLIENT_DEFAULT_MAX_CONN;

    if (config.timeout_ms == 0)
        config.timeout_ms = MESH_CLIENT_DEFAULT_TIMEOUT_MS;
}

int ClientContext::init()
{
    grpc_client = mesh_internal_ops.grpc_create_client();

    return 0;
}

int ClientContext::shutdown()
{
    std::lock_guard<std::mutex> lk(mx);

    if (!conns.empty())
        return -MESH_ERR_FOUND_ALLOCATED;

    /**
     * TODO: Shutdown and deallocate connections here
     */

    mesh_internal_ops.grpc_destroy_client(grpc_client);
    grpc_client = NULL;

    return 0;
}

int ClientContext::create_conn(MeshConnection **conn)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    std::lock_guard<std::mutex> lk(mx);

    if ((long int)conns.size() >= config.max_conn_num)
        return -MESH_ERR_MAX_CONN;

    ConnectionContext *conn_ctx = new(std::nothrow) ConnectionContext(this);
    if (!conn_ctx)
        return -ENOMEM;

    try {
        conns.push_back(conn_ctx);
    }
    catch (...) {
        delete conn_ctx;
        return -ENOMEM;
    }

    *conn = (MeshConnection *)conn_ctx;

    return 0;
}

} // namespace mesh
