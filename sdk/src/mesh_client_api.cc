/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mesh_client_api.h"

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "sdk.grpc.pb.h"
#include "mesh_logger.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using sdk::ClientAPI;
using sdk::CreateConnectionRequest;
using sdk::CreateConnectionResponse;
using sdk::DeleteConnectionRequest;
using sdk::DeleteConnectionResponse;

using namespace mesh;

class ClientAPIClient {
public:
    ClientAPIClient(std::shared_ptr<Channel> channel)
        : stub_(ClientAPI::NewStub(channel)) {}

    int CreateConnection(std::string& conn_id, mcm_conn_param *param,
                         memif_conn_param *memif_param) {
        if (!param || !memif_param)
            return -1;

        CreateConnectionRequest req;
        req.set_client_id(client_id);

        std::string param_str(reinterpret_cast<const char *>(param),
                              sizeof(mcm_conn_param));
        req.set_mcm_conn_param(param_str);

        CreateConnectionResponse resp;
        ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(5));

        Status status = stub_->CreateConnection(&context, req, &resp);

        if (status.ok()) {
            client_id = resp.client_id();
            conn_id = resp.conn_id();

            int sz = resp.memif_conn_param().size();
            if (sz != sizeof(memif_conn_param)) {
                log::error("Param size (%d) not equal to memif_conn_param (%ld)",
                           sz, sizeof(memif_conn_param));
                return -1;
            }

            memcpy(memif_param, resp.memif_conn_param().data(), sz);

            return 0;
        } else {
            log::error("CreateConnection RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }
    }

    int DeleteConnection(const std::string& conn_id) {
        DeleteConnectionRequest req;
        req.set_client_id(client_id);
        req.set_conn_id(conn_id);

        DeleteConnectionResponse resp;
        ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(5));

        Status status = stub_->DeleteConnection(&context, req, &resp);

        if (status.ok()) {
            return 0;
        } else {
            log::error("DeleteConnection RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }
    }

    std::string client_id;

private:
    std::unique_ptr<ClientAPI::Stub> stub_;
};

void * mesh_grpc_create_client()
{
    auto client = new(std::nothrow)
        ClientAPIClient(grpc::CreateChannel("localhost:50050", // gRPC default 50051
                        grpc::InsecureChannelCredentials()));
    return client;
}

void mesh_grpc_destroy_client(void *client)
{
    if (client) {
        auto cli = static_cast<ClientAPIClient *>(client);
        delete cli;
    }
}

class GrpcConn {
public:
    // This declaration must go first to allow proper type casting.
    mcm_conn_context *handle;

    ClientAPIClient *client;
    std::string conn_id;
};

// Can't include the entire header file due to the C/C++ atomics incompatibility.
extern "C"
mcm_conn_context* mcm_create_connection_memif(mcm_conn_param* svc_args,
                                              memif_conn_param* memif_args);

void * mesh_grpc_create_conn(void *client, mcm_conn_param *param)
{
    if (!client)
        return NULL;

    auto cli = static_cast<ClientAPIClient *>(client);

    auto conn = new(std::nothrow) GrpcConn();
    if (!conn)
        return NULL;

    memif_conn_param memif_param = {};

    int err = cli->CreateConnection(conn->conn_id, param, &memif_param);
    if (err) {
        delete conn;
        log::error("Create gRPC connection failed (%d)", err);
        return NULL;
    }

    log::info("gRPC: connection created")("id", conn->conn_id)
                                         ("client_id", cli->client_id);

    conn->client = cli;

    // Connect memif connection
    // TODO: Propagate the main context to enable cancellation.
    conn->handle = mcm_create_connection_memif(param, &memif_param);
    if (!conn->handle) {
        delete conn;
        log::error("gRPC: failed to create memif interface");
        return NULL;
    }

    return conn;
}

void mesh_grpc_destroy_conn(void *conn_ptr)
{
    if (!conn_ptr)
        return;

    auto conn = static_cast<GrpcConn *>(conn_ptr);

    int err = conn->client->DeleteConnection(conn->conn_id);
    if (err)
        log::error("Delete gRPC connection failed (%d)", err);

    log::info("gRPC: connection deleted")("id", conn->conn_id);

    delete conn;
}

