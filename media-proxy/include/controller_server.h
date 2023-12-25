/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "controller.grpc.pb.h"
#include "proxy_context.h"
#include <condition_variable>
#include <grpc++/grpc++.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#pragma once

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using controller::Configure;
using controller::ControlReply;
using controller::RxControlRequest;
using controller::St20pRxOps;
using controller::StInit;
using controller::StopControlRequest;
using controller::StRxPort;
using controller::TxControlRequest;

using controller::MsmDataPlane;
using controller::StreamData;
using controller::StreamResult;

using controller::Health;
using controller::HealthCheckRequest;
using controller::HealthCheckResponse;

class ConfigureServiceImpl final : public Configure::Service {
public:
    ConfigureServiceImpl(ProxyContext* ctx);
    Status TxStart(ServerContext* context, const TxControlRequest* request, ControlReply* reply) override;
    Status RxStart(ServerContext* context, const RxControlRequest* request, ControlReply* reply) override;
    Status TxStop(ServerContext* context, const StopControlRequest* request, ControlReply* reply) override;
    Status RxStop(ServerContext* context, const StopControlRequest* request, ControlReply* reply) override;
    Status Stop(ServerContext* context, const StopControlRequest* request, ControlReply* reply) override;

private:
    ProxyContext* m_ctx;
};

class MsmDataPlaneServiceImpl final : public MsmDataPlane::Service {
public:
    MsmDataPlaneServiceImpl(ProxyContext* ctx);
    Status stream_add_del(ServerContext* context, const StreamData* request, StreamResult* reply) override;

private:
    ProxyContext* m_ctx;
};

class HealthServiceImpl final : public Health::Service {
public:
    HealthServiceImpl(ProxyContext* ctx);
    Status Check(ServerContext* context, const HealthCheckRequest* request, HealthCheckResponse* reply) override;
    Status Watch(ServerContext* context, const HealthCheckRequest* request, HealthCheckResponse* reply) override;

private:
    ProxyContext* m_ctx;
};

void RunRPCServer(ProxyContext* ctx);
