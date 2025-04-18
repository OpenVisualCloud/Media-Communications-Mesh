/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "proxy_api.h"

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <sstream>
#include "logger.h"
#include "manager_multipoint.h"
#include "proxy_config.h"
#include "manager_local.h"

namespace mesh {

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;
using mediaproxy::ProxyAPI;
using mediaproxy::RegisterMediaProxyRequest;
using mediaproxy::RegisterMediaProxyReply;
using mediaproxy::UnregisterMediaProxyRequest;
using mediaproxy::UnregisterMediaProxyReply;
using mediaproxy::RegisterConnectionRequest;
using mediaproxy::RegisterConnectionReply;
using mediaproxy::UnregisterConnectionRequest;
using mediaproxy::UnregisterConnectionReply;
using mediaproxy::SendMetricsRequest;
using mediaproxy::SendMetricsReply;
using mediaproxy::Metric;
using mediaproxy::MetricField;
using mediaproxy::StartCommandQueueRequest;
using mediaproxy::CommandRequest;
using mediaproxy::CommandReplyReceipt;
using mediaproxy::DebugReply;
using mediaproxy::ApplyConfigReply;
using mediaproxy::Bridge;
using mediaproxy::ST2110ProxyConfig;
using mediaproxy::RDMAProxyConfig;
using sdk::ConnectionConfig;

std::unique_ptr<ProxyAPIClient> proxyApiClient;

int ProxyAPIClient::RegisterMediaProxy()
{
    RegisterMediaProxyRequest request;
    request.set_sdk_api_port(config::proxy.sdk_api_port);

    ST2110ProxyConfig* st2110_cfg = request.mutable_st2110_config();
    st2110_cfg->set_dev_port_bdf(config::proxy.st2110.dev_port_bdf);
    st2110_cfg->set_dataplane_ip_addr(config::proxy.st2110.dataplane_ip_addr);

    RDMAProxyConfig* rdma_cfg = request.mutable_rdma_config();
    rdma_cfg->set_dataplane_ip_addr(config::proxy.rdma.dataplane_ip_addr);
    rdma_cfg->set_dataplane_local_ports(config::proxy.rdma.dataplane_local_ports);

    RegisterMediaProxyReply reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(15)); // Increased from 5 to 15 as a workaround for k8s

    Status status = stub_->RegisterMediaProxy(&context, request, &reply);

    if (status.ok()) {
        SetProxyId(reply.proxy_id());
        return 0;
    } else {
        log::error("RegisterMediaProxy RPC failed: %s",
                   status.error_message().c_str());
        return -1;
    }
}

int ProxyAPIClient::UnregisterMediaProxy()
{
    UnregisterMediaProxyRequest request;
    request.set_proxy_id(GetProxyId());

    UnregisterMediaProxyReply reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(5));

    Status status = stub_->UnregisterMediaProxy(&context, request, &reply);

    if (status.ok()) {
        return 0;
    } else {
        log::error("UnregisterMediaProxy RPC failed: %s",
                   status.error_message().c_str());
        return -1;
    }
}

int ProxyAPIClient::RegisterConnection(std::string& conn_id, const std::string& kind,
                                       const connection::Config& config,
                                       std::string& err)
{
    RegisterConnectionRequest request;
    request.set_proxy_id(GetProxyId());
    request.set_kind(kind);
    request.set_conn_id(conn_id); // Normally should be empty.

    ConnectionConfig* cfg = request.mutable_config();
    config.assign_to_pb(*cfg);

    RegisterConnectionReply reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                        std::chrono::seconds(5));

    Status status = stub_->RegisterConnection(&context, request, &reply);

    if (status.ok()) {
        conn_id = reply.conn_id();
        return 0;
    } else {
        log::error("RegisterConnection RPC failed: %s",
                   status.error_message().c_str());
        err = status.error_message();
        return -1;
    }
}

int ProxyAPIClient::UnregisterConnection(const std::string& conn_id)
{
    UnregisterConnectionRequest request;
    request.set_conn_id(conn_id);
    request.set_proxy_id(GetProxyId());

    UnregisterConnectionReply reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(5));

    Status status = stub_->UnregisterConnection(&context, request, &reply);

    if (status.ok()) {
        return 0;
    } else {
        log::error("UnregisterConnection RPC failed: %s",
                   status.error_message().c_str());
        return -1;
    }
}

int ProxyAPIClient::SendMetrics(const std::vector<telemetry::Metric>& metrics)
{
    auto proxy_id = GetProxyId();
    if (proxy_id.empty())
        return 0;

    SendMetricsRequest request;

    request.set_proxy_id(proxy_id);

    for (const auto& metric : metrics) {
        Metric* out_metric = request.add_metrics();
        out_metric->set_timestamp_ms(metric.timestamp_ms);
        out_metric->set_provider_id(metric.provider_id);

        for (const auto& field : metric.fields) {
            MetricField* out_field = out_metric->add_fields();
            out_field->set_name(field.name);
            if (std::holds_alternative<std::string>(field.value)) {
                out_field->set_str_value(std::get<std::string>(field.value));
            } else if (std::holds_alternative<uint64_t>(field.value)) {
                out_field->set_uint_value(std::get<uint64_t>(field.value));
            } else if (std::holds_alternative<double>(field.value)) {
                out_field->set_double_value(std::get<double>(field.value));
            } else if (std::holds_alternative<bool>(field.value)) {
                out_field->set_bool_value(std::get<bool>(field.value));
            }
        }
    }

    SendMetricsReply reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(5));

    Status status = stub_->SendMetrics(&context, request, &reply);

    if (!status.ok()) {
        auto err = status.error_code();
        if (err == grpc::StatusCode::UNAVAILABLE ||
            err == grpc::StatusCode::NOT_FOUND)
            return -1;

        log::error("Failed to send metrics: %s",
                   status.error_message().c_str());
        return -1;
    }
    return 0;
}

int ProxyAPIClient::SendCommandReply(CommandReply& request)
{
    CommandReplyReceipt reply;

    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(5));

    Status status = stub_->SendCommandReply(&context, request, &reply);

    if (!status.ok()) {
        log::error("SendCommandReply RPC failed: %s",
                   status.error_message().c_str());
        return -1;
    }

    // log::debug("Command reply sent successfully");
    return 0;
}

int ProxyAPIClient::StartCommandQueue(context::Context& ctx)
{
    StartCommandQueueRequest request;
    request.set_proxy_id(GetProxyId());

    ClientContext context;

    auto cctx = context::WithCancel(ctx);

    std::jthread shutdown_thread([&]() {
        cctx.done();
        context.TryCancel();
    });

    thread::Defer d([&]{ cctx.cancel(); });

    std::unique_ptr<ClientReader<CommandRequest>> reader(
        stub_->StartCommandQueue(&context, request));

    CommandRequest command_request;
    while (reader->Read(&command_request)) {
        CommandReply result_request;
        result_request.set_req_id(command_request.req_id());
        result_request.set_proxy_id(GetProxyId());

        switch (command_request.command_case()) {

        case CommandRequest::kDebug:
            {
                log::debug("Received Debug command: %s",
                           command_request.debug().in_text().c_str())
                          ("req_id", command_request.req_id());


                // Create and populate the CommandReply message
                DebugReply* reply = new DebugReply();
                reply->set_out_text("Okay Okay!");
                result_request.set_allocated_debug(reply);

                SendCommandReply(result_request);
            }
            break;

        case CommandRequest::kApplyConfig:
            {
                auto& req = command_request.apply_config();

                multipoint::Config config;

                log::info("[AGENT] ApplyConfig")
                         ("groups", req.groups_size())
                         ("bridges", req.bridges_size());

                for (const auto& group : req.groups()) {
                    multipoint::GroupConfig group_config;

                    log::info("* Group")
                             ("group_id", group.group_id())
                             ("conns", group.conn_ids_size())
                             ("bridges", group.bridge_ids_size());

                    for (const auto& conn_id : group.conn_ids()) {
                        // log::debug("-- Conn")("conn_id", conn_id);
                        group_config.conn_ids.emplace_back(conn_id);
                    }
                    for (const auto& bridge_id : group.bridge_ids()) {
                        // log::debug("-- Bridge")("bridge_id", bridge_id);
                        group_config.bridge_ids.emplace_back(bridge_id);
                    }

                    config.groups[group.group_id()] = group_config;
                }

                for (const auto& bridge : req.bridges()) {
                    log::info("* Bridge")
                             ("bridge_id", bridge.bridge_id())
                             ("type", bridge.type())
                             ("kind", bridge.kind());

                    connection::Kind kind;
                    if (!bridge.kind().compare("tx")) {
                        kind = connection::Kind::transmitter;
                    } else if (!bridge.kind().compare("rx")) {
                        kind = connection::Kind::receiver;
                    } else {
                        log::error("Bad bridge kind: '%s'", bridge.kind().c_str())
                                  ("bridge_id", bridge.bridge_id())
                                  ("type", bridge.type());
                        continue;
                    }

                    const auto& bridge_id = bridge.bridge_id();

                    multipoint::BridgeConfig bridge_config = {
                        .type = bridge.type(),
                        .kind = kind,
                    };

                    if (bridge.has_conn_config())
                        bridge_config.conn_config.assign_from_pb(bridge.conn_config());
                    else
                        log::error("No conn config for bridge")
                                  ("bridge_id", bridge.bridge_id());

                    switch (bridge.config_case()) {

                    case Bridge::kSt2110:
                        if (bridge_config.type.compare("st2110")) {
                            log::error("st2110 bridge config provided for type '%s'",
                                       bridge_config.type.c_str());
                        } else {
                            auto& req = bridge.st2110();
                            
                            log::info("** ST2110")
                                     ("ip_addr", req.ip_addr())
                                     ("port", req.port())
                                     ("mcast_sip_addr", req.mcast_sip_addr())
                                     ("transport", req.transport());

                            bridge_config.st2110.ip_addr = req.ip_addr();
                            bridge_config.st2110.port = req.port();
                            bridge_config.st2110.mcast_sip_addr = req.mcast_sip_addr();
                            bridge_config.st2110.transport = req.transport();
                            bridge_config.st2110.payload_type = req.payload_type();

                            config.bridges[bridge_id] = std::move(bridge_config);
                        }
                        break;

                    case Bridge::kRdma:
                        if (bridge_config.type.compare("rdma")) {
                            log::error("rdma bridge config provided for type '%s'",
                                       bridge_config.type.c_str());
                        } else {
                            auto& req = bridge.rdma();
                            
                            log::info("** RDMA")
                                     ("remote_ip_addr", req.remote_ip_addr())
                                     ("port", req.port());

                            bridge_config.rdma.remote_ip_addr = req.remote_ip_addr();
                            bridge_config.rdma.port = req.port();

                            config.bridges[bridge_id] = std::move(bridge_config);
                        }
                        break;
                    
                    default:
                        log::error("Unknown bridge configuration")
                                  ("bridge_id", bridge.bridge_id());
                        break;

                    }
                }

                ApplyConfigReply* reply = new ApplyConfigReply();
                result_request.set_allocated_apply_config(reply);

                SendCommandReply(result_request);

                // Apply config after the ApplyConfig command confirmation
                // has been sent to avoid mutual nested locking.
                multipoint::group_manager.apply_config(ctx, config);
                // TODO: Do we need to check and return the result here?
            }
            break;

            // case CommandRequest::kResetMetrics:
            //     std::cout << "Received Reset Metrics Command: " << request->reset_metrics().metric_id() << std::endl;
            //     break;

        default:
            log::error("Unknown proxy command")
                      ("req_id", command_request.req_id());
            break;
        }

    }

    Status status = reader->Finish();
    if (!status.ok()) {
        switch (status.error_code()) {
        case grpc::StatusCode::CANCELLED:
            return 0;

        case grpc::StatusCode::NOT_FOUND:
            SetProxyId("");
            log::info("Trigger Media Proxy registration");
            return 0;

        default:
            log::error("StartCommandQueue RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }
    }

    return 0;
}

int ProxyAPIClient::Run(context::Context& ctx)
{
    try {
        th = std::jthread([&]() {
            while (!ctx.cancelled()) {
                if (GetProxyId().empty()) {
                    auto err = RegisterMediaProxy();
                    if (err) {
                        thread::Sleep(ctx, std::chrono::milliseconds(2000));
                        continue;
                    }
                    log::info("Media Proxy registered")
                             ("proxy_id", GetProxyId());

                    // connection::local_manager.reregister_all_connections(ctx);
                    
                    // Close all existing connections
                    connection::local_manager.shutdown(ctx);
                }

                auto err = StartCommandQueue(ctx);
                if (err) {
                    thread::Sleep(ctx, std::chrono::milliseconds(2000));
                    continue;
                }
            }
        });
    }
    catch (const std::system_error& e) {
        log::error("Proxy API thread create failed (out of memory)");
        return -1;
    }

    return 0;
}

void ProxyAPIClient::Shutdown()
{
    UnregisterMediaProxy();

    th.join();
}

void ProxyAPIClient::SetProxyId(const std::string& id)
{
    std::lock_guard<std::mutex> lk(_proxy_id_mx);
    _proxy_id = id;
}

std::string ProxyAPIClient::GetProxyId()
{
    std::lock_guard<std::mutex> lk(_proxy_id_mx);
    return _proxy_id;
}

int RunProxyAPIClient(context::Context& ctx)
{
    proxyApiClient = std::make_unique<ProxyAPIClient>(
        grpc::CreateChannel(config::proxy.agent_addr,
                            grpc::InsecureChannelCredentials()));

    return proxyApiClient->Run(ctx);
}

} // namespace mesh
