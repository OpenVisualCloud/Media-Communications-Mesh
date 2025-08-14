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
#include "client_registry.h"
#include "uuid.h"
#include "event.h"
#include "manager_multipoint.h"

namespace mesh {

using ::grpc::Server;
using ::grpc::ServerBuilder;
using ::grpc::ServerWriter;
using ::grpc::ServerContext;
using ::grpc::Status;
using ::grpc::StatusCode;
using ::sdk::SDKAPI;
using ::sdk::CreateConnectionRequest;
using ::sdk::CreateConnectionResponse;
using ::sdk::CreateConnectionZeroCopyResponse;
using ::sdk::ActivateConnectionRequest;
using ::sdk::ActivateConnectionResponse;
using ::sdk::DeleteConnectionRequest;
using ::sdk::DeleteConnectionResponse;
using ::sdk::RegisterRequest;
using ::sdk::Event;
using ::sdk::SendMetricsRequest;
using ::sdk::ConnectionMetrics;
using ::sdk::SendMetricsResponse;
using ::sdk::ConnectionConfig;
using ::sdk::ConfigMultipointGroup;
using ::sdk::ConfigST2110;
using ::sdk::ConfigRDMA;
using ::sdk::ConfigVideo;
using ::sdk::ConfigAudio;
using ::sdk::ST2110Transport;
using ::sdk::VideoPixelFormat;
using ::sdk::AudioSampleRate;
using ::sdk::AudioFormat;
using ::sdk::AudioPacketTime;

class SDKAPIServiceImpl final : public SDKAPI::Service {
public:
    Status CreateConnection(ServerContext* sctx, const CreateConnectionRequest* req,
                            CreateConnectionResponse* resp) override {
        const auto& client_id = req->client_id();
        auto client = client::registry.get(client_id);
        if (!client)
            return Status(StatusCode::INVALID_ARGUMENT, "client not registered");

        std::string name = req->name();

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

        auto ctx = context::WithCancel(context::Background());
        std::string conn_id;
        std::string err_str;

        memif_conn_param memif_param = {};
        auto& mgr = connection::local_manager;
        int err = mgr.create_connection_sdk(ctx, conn_id, client_id, memif_param,
                                            conn_config, name, err_str);
        if (err) {
            log::error("create_connection_sdk() failed (%d)", err);
            if (err_str.empty())
                return Status(StatusCode::INTERNAL, "create_connection_sdk() failed");
            else
                return Status(StatusCode::INTERNAL, err_str);
        }

        memif_param.conn_args.is_master = 0; // SDK client is to be secondary

        resp->set_conn_id(conn_id);
        std::string memif_param_str(reinterpret_cast<const char *>(&memif_param),
                                    sizeof(memif_conn_param));
        resp->set_memif_conn_param(memif_param_str);

        log::info("[SDK] Connection created")("id", resp->conn_id())
                                             ("client_id", client_id);
        return Status::OK;
    }

    Status CreateConnectionZeroCopy(ServerContext* sctx, const CreateConnectionRequest* req,
                                    CreateConnectionZeroCopyResponse* resp) override {
        const auto& client_id = req->client_id();
        auto client = client::registry.get(client_id);
        if (!client)
            return Status(StatusCode::INVALID_ARGUMENT, "client not registered");

        std::string name = req->name();

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

        auto ctx = context::WithCancel(context::Background());
        std::string conn_id;
        std::string err_str;

        const auto& temporary_id = req->temporary_id();

        auto& mgr = connection::local_manager;
        int err = mgr.create_connection_zc_sdk(ctx, conn_id, client_id,
                                               conn_config, name, temporary_id,
                                               err_str);
        if (err) {
            log::error("create_connection_zc_sdk() failed (%d)", err);
            if (err_str.empty())
                return Status(StatusCode::INTERNAL, "create_connection_zc_sdk() failed");
            else
                return Status(StatusCode::INTERNAL, err_str);
        }

        resp->set_conn_id(conn_id);

        log::info("[SDK] Connection ZC created")("id", conn_id)
                                                ("temporary_id", temporary_id)
                                                ("client_id", client_id);
        return Status::OK;
    }

    Status ActivateConnection(ServerContext* sctx, const ActivateConnectionRequest* req,
                              ActivateConnectionResponse* resp) override {
        const auto& client_id = req->client_id();
        auto client = client::registry.get(client_id);
        if (!client)
            return Status(StatusCode::INVALID_ARGUMENT, "client not registered");
                        
        auto ctx = context::WithCancel(context::Background());
        auto conn_id = req->conn_id();

        auto& mgr = connection::local_manager;
        auto ret = mgr.activate_connection_sdk(ctx, conn_id);
        switch (ret) {
        case connection::Result::success:
            log::info("[SDK] Connection active")("id", req->conn_id())
                                                ("client_id", client_id);
            resp->set_linked(true);
            return Status::OK;
        
        case connection::Result::error_no_link_assigned:
            resp->set_linked(false);
            return Status::OK;

        default:
            return Status(StatusCode::INTERNAL, connection::result2str(ret));
        }
    }

    Status DeleteConnection(ServerContext* sctx, const DeleteConnectionRequest* req,
                            DeleteConnectionResponse* resp) override {
        const auto& client_id = req->client_id();
        auto client = client::registry.get(client_id);
        if (!client)
            return Status(StatusCode::INVALID_ARGUMENT, "client not registered");

        auto ctx = context::WithCancel(context::Background());
        auto conn_id = req->conn_id();

        auto& mgr = connection::local_manager;
        int err = mgr.delete_connection_sdk(ctx, conn_id);
        if (err)
            ; // log::error("delete_local_conn err (%d)", err);
        else
            log::info("[SDK] Connection deleted")("id", conn_id)
                                                 ("client_id", client_id);

        return Status::OK;
    }

    Status SendMetrics(ServerContext* sctx, const SendMetricsRequest* req,
                       SendMetricsResponse* resp) override {
        const auto& client_id = req->client_id();

        auto client = client::registry.get(client_id);
        if (!client)
            return Status(StatusCode::INVALID_ARGUMENT, "client not registered");

        auto ctx = context::WithCancel(context::Background());

        for (const auto& metric : req->conn_metrics()) {

            auto& conn_id = metric.conn_id();

            auto& mgr = connection::local_manager;
            mgr.lock();
            auto conn = mgr.find_connection(ctx, conn_id);
            if (!conn) {
                mgr.unlock();
                continue;
            }

            conn->metrics.inbound_bytes = metric.inbound_bytes();
            conn->metrics.outbound_bytes = metric.outbound_bytes();
            conn->metrics.transactions_succeeded = metric.transactions_succeeded();
            conn->metrics.transactions_failed = metric.transactions_failed();
            conn->metrics.errors = metric.errors();

            mgr.unlock();
            
            // log::debug("Metric")
            //           ("client_id", client_id)
            //           ("conn_id", conn_id)
            //           ("in", metric.inbound_bytes())
            //           ("out", metric.outbound_bytes())
            //           ("strn", metric.transactions_succeeded())
            //           ("ftrn", metric.transactions_failed())
            //           ("err", metric.errors());
        }
        return Status::OK;
    }
    
    Status RegisterAndStreamEvents(ServerContext* sctx, const RegisterRequest* req,
                                   ServerWriter<Event>* writer) override {
        std::string id;
        auto status = register_client(id);
        if (!status.ok()) {
            log::error("SDK client registration err: %s", status.error_message().c_str());
            return status;
        }

        auto ch = event::broker.subscribe(id);
        if (!ch) {
            log::error("SDK client event subscription failed");
            return status;
        }

        Event event;
        event.mutable_client_registered()->set_client_id(id);
        writer->Write(event);

        while (!sctx->IsCancelled() && !ctx.cancelled()) {
            auto tctx = context::WithTimeout(ctx, std::chrono::milliseconds(500));
            auto v = ch->receive(tctx);
            if (!v.has_value())
                continue;

            auto evt = v.value();
            // log::debug("Sending event")("type", (int)evt.type);

            Event event;

            if (evt.type == event::Type::conn_unlink_requested) {
                auto e = event.mutable_conn_unlink_requested();
                if (evt.params.contains("conn_id")) {
                    auto v = evt.params["conn_id"];
                    if (v.type() == typeid(std::string)) {
                        auto conn_id = std::any_cast<std::string>(v);
                        e->set_conn_id(conn_id);
                    }
                }
            } else if (evt.type == event::Type::conn_zero_copy_config) {
                auto e = event.mutable_conn_zero_copy_config();
                if (evt.params.contains("conn_id")) {
                    auto v = evt.params["conn_id"];
                    if (v.type() == typeid(std::string)) {
                        auto conn_id = std::any_cast<std::string>(v);
                        e->set_conn_id(conn_id);
                    }
                }
                if (evt.params.contains("temporary_id")) {
                    auto v = evt.params["temporary_id"];
                    if (v.type() == typeid(std::string)) {
                        auto temporary_id = std::any_cast<std::string>(v);
                        e->set_temporary_id(temporary_id);
                    }
                }
                if (evt.params.contains("sysv_key")) {
                    auto v = evt.params["sysv_key"];
                    if (v.type() == typeid(key_t)) {
                        auto sysv_key = std::any_cast<key_t>(v);
                        e->set_sysv_key(sysv_key);
                    }
                }
                if (evt.params.contains("mem_region_sz")) {
                    auto v = evt.params["mem_region_sz"];
                    if (v.type() == typeid(size_t)) {
                        auto mem_region_sz = std::any_cast<size_t>(v);
                        e->set_mem_region_sz(mem_region_sz);
                    }
                }
            } else if (evt.type == event::Type::report_metrics_triggered) {
                auto e = event.mutable_report_metrics_triggered();
            }

            writer->Write(event);
        }
        
        event::broker.unsubscribe(ch);

        unregister_client(id);

        return Status::OK;
    }

    context::Context ctx = context::WithCancel(context::Background());

private:
    Status register_client(std::string& id) {
        auto client = new(std::nothrow) client::Client;
        if (!client)
            return Status(StatusCode::INTERNAL, "out of memory");

        bool found = false;
        for (int i = 0; i < 5; i++) {
            id = generate_uuid_v4();
            client->id = id;
            int err = client::registry.add(id, client);
            if (!err) {
                found = true;
                break;
            }
        }
        if (!found) {
            delete client;
            log::error("SDK client registry contains UUID, max attempts.");
            return Status(StatusCode::INTERNAL, "UUID max attempts");
        }
    
        return Status::OK;
    }

    void unregister_client(const std::string& id) {
        auto client = client::registry.get(id);
        if (!client) {
            log::error("SDK client unregister: id not found");
            return;
        }
        
        client::registry.remove(id);
        delete client;
    }
};

void RunSDKAPIServer(context::Context& ctx) {
    std::string server_address("0.0.0.0:"); // gRPC default 50051
    server_address += std::to_string(config::proxy.sdk_api_port);
    SDKAPIServiceImpl service;
    service.ctx = context::WithCancel(ctx);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    log::info("SDK API Server listening on %s", server_address.c_str());

    std::jthread th([&]() {
        service.ctx.done();
        log::info("Shutting down SDK API Server");
        server->Shutdown();
    });

    server->Wait();
    service.ctx.cancel();
}

} // namespace mesh
