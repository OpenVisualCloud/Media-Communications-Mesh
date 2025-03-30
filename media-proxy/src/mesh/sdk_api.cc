/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iostream>
#include <memory>
#include <string>

#include "sdk_api.h"
#include <grpcpp/grpcpp.h>
#include "sdk.grpc.pb.h"
#include "logger.h"
#include "mcm_dp.h"
#include "manager_local.h"
#include "proxy_config.h"

namespace mesh {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using sdk::SDKAPI;
using sdk::CreateConnectionRequest;
using sdk::CreateConnectionResponse;
using sdk::ActivateConnectionRequest;
using sdk::ActivateConnectionResponse;
using sdk::DeleteConnectionRequest;
using sdk::DeleteConnectionResponse;
using sdk::ConnectionConfig;
using sdk::ConfigMultipointGroup;
using sdk::ConfigST2110;
using sdk::ConfigRDMA;
using sdk::ConfigVideo;
using sdk::ConfigAudio;
using sdk::ST2110Transport;
using sdk::VideoPixelFormat;
using sdk::AudioSampleRate;
using sdk::AudioFormat;
using sdk::AudioPacketTime;

class SDKAPIServiceImpl final : public SDKAPI::Service {
public:
    Status CreateConnection(ServerContext* sctx, const CreateConnectionRequest* req,
                            CreateConnectionResponse* resp) override {
        memif_conn_param memif_param = {};
        mcm_conn_param param = {};

        // log::debug("Has config?")("yes", req->has_config());

        if (!req->has_config()) {
            log::error("SDK: no config provided");
            return Status(StatusCode::INVALID_ARGUMENT, "no config provided");
        }

        connection::Config conn_config;
        auto res = conn_config.assign_from_pb(req->config());
        if (res != connection::Result::success) {
            log::error("SDK: parse err: %s", connection::result2str(res));
            return Status(StatusCode::INVALID_ARGUMENT, connection::result2str(res));
        }

        int sz = req->mcm_conn_param().size();
        if (sz != sizeof(mcm_conn_param)) {
            log::debug("Param size (%d) not equal to mcm_conn_param (%ld)",
                       sz, sizeof(mcm_conn_param));
            // return Status(StatusCode::INVALID_ARGUMENT,
            //               "Wrong size of mcm_conn_param");
        }
        memcpy(&param, req->mcm_conn_param().data(), sz);

        auto ctx = context::WithCancel(context::Background());
        std::string conn_id;
        std::string err_str;

        auto& mgr = connection::local_manager;
        int err = mgr.create_connection_sdk(ctx, conn_id, &param, &memif_param,
                                            conn_config, err_str);
        if (err) {
            log::error("create_local_conn() failed (%d)", err);
            if (err_str.empty())
                return Status(StatusCode::INTERNAL, "create_local_conn() failed");
            else
                return Status(StatusCode::INTERNAL, err_str);
        }

        memif_param.conn_args.is_master = 0; // SDK client is to be secondary

        resp->set_conn_id(conn_id);
        resp->set_client_id("default-client");
        std::string memif_param_str(reinterpret_cast<const char *>(&memif_param),
                                    sizeof(memif_conn_param));
        resp->set_memif_conn_param(memif_param_str);

        log::info("[SDK] Connection created")("id", resp->conn_id())
                                             ("client_id", resp->client_id());
        return Status::OK;
    }

    Status ActivateConnection(ServerContext* sctx, const ActivateConnectionRequest* req,
                              ActivateConnectionResponse* resp) override {

        auto ctx = context::WithCancel(context::Background());
        auto conn_id = req->conn_id();

        auto& mgr = connection::local_manager;
        int err = mgr.activate_connection_sdk(ctx, conn_id);
        if (err)
            ; // log::error("activate_local_conn err (%d)", err);
        else
            log::info("[SDK] Connection active")("id", req->conn_id())
                                                ("client_id", req->client_id());

        return Status::OK;
    }

    Status DeleteConnection(ServerContext* sctx, const DeleteConnectionRequest* req,
                            DeleteConnectionResponse* resp) override {

        auto ctx = context::WithCancel(context::Background());
        auto conn_id = req->conn_id();

        auto& mgr = connection::local_manager;
        int err = mgr.delete_connection_sdk(ctx, conn_id);
        if (err)
            ; // log::error("delete_local_conn err (%d)", err);
        else
            log::info("[SDK] Connection deleted")("id", req->conn_id())
                                                 ("client_id", req->client_id());

        return Status::OK;
    }
};

void RunSDKAPIServer(context::Context& ctx) {
    std::string server_address("0.0.0.0:"); // gRPC default 50051
    server_address += std::to_string(config::proxy.sdk_api_port);
    SDKAPIServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    log::info("SDK API Server listening on %s", server_address.c_str());

    std::jthread th([&]() {
        ctx.done();
        log::info("Shutting down SDK API Server");
        server->Shutdown();
    });

    server->Wait();
}

} // namespace mesh
