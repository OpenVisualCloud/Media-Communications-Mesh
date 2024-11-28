/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iostream>
#include <memory>
#include <string>

#include "client_api.h"
#include <grpcpp/grpcpp.h>
#include "sdk.grpc.pb.h"
#include "logger.h"
#include "mcm_dp.h"
#include "manager_local.h"

namespace mesh {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using sdk::ClientAPI;
using sdk::CreateConnectionRequest;
using sdk::CreateConnectionResponse;
using sdk::DeleteConnectionRequest;
using sdk::DeleteConnectionResponse;

class ClientAPIServiceImpl final : public ClientAPI::Service {
public:
    Status CreateConnection(ServerContext* sctx, const CreateConnectionRequest* req,
                            CreateConnectionResponse* resp) override {
        memif_conn_param memif_param = {};
        mcm_conn_param param = {};

        int sz = req->mcm_conn_param().size();
        if (sz != sizeof(mcm_conn_param)) {
            log::error("Param size (%d) not equal to mcm_conn_param (%ld)",
                       sz, sizeof(mcm_conn_param));
            return Status(StatusCode::INVALID_ARGUMENT,
                          "Wrong size of mcm_conn_param");
        }
        memcpy(&param, req->mcm_conn_param().data(), sz);

        auto ctx = context::WithCancel(context::Background());
        std::string conn_id;

        auto& mgr = connection::local_manager;
        int err = mgr.create_connection(ctx, conn_id, &param, &memif_param);
        if (err) {
            log::error("create_local_conn() failed (%d)", err);
            return Status(StatusCode::INTERNAL,
                          "create_local_conn() failed");
        }

        memif_param.conn_args.is_master = 0; // SDK client is to be secondary

        resp->set_conn_id(conn_id);
        resp->set_client_id("default-client");
        std::string memif_param_str(reinterpret_cast<const char *>(&memif_param),
                                    sizeof(memif_conn_param));
        resp->set_memif_conn_param(memif_param_str);

        log::info("Connection created")("id", resp->conn_id())
                                       ("client_id", resp->client_id());
        return Status::OK;
    }

    Status DeleteConnection(ServerContext* sctx, const DeleteConnectionRequest* req,
                            DeleteConnectionResponse* resp) override {

        auto ctx = context::WithCancel(context::Background());
        const auto& conn_id = req->conn_id();

        auto& mgr = connection::local_manager;
        int err = mgr.delete_connection(ctx, conn_id);
        if (err)
            log::error("delete_local_conn err (%d)", err);
        else
            log::info("Connection deleted")("id", req->conn_id())
                                           ("client_id", req->client_id());

        return Status::OK;
    }
};

void RunClientAPIServer(context::Context& ctx) {
    std::string server_address("0.0.0.0:50050"); // gRPC default 50051
    ClientAPIServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    log::info("Client API Server listening on %s", server_address.c_str());

    std::jthread th([&]() {
        ctx.done();
        log::info("Shutting down Client API Server");
        server->Shutdown();
    });

    server->Wait();
}

} // namespace mesh
