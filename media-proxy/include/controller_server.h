/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iostream>
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>
#include <grpc++/grpc++.h>
#include "controller.grpc.pb.h"
#include "proxy_context.h"

#pragma once

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using controller::TxControlRequest;
using controller::RxControlRequest;
using controller::StInit;
using controller::St20pRxOps;
using controller::StRxPort;
using controller::StopControlRequest;
using controller::ControlReply;
using controller::Configure;

class ConfigureServiceImpl final : public Configure::Service {
public:
    ConfigureServiceImpl(ProxyContext *ctx);
    Status TxStart(ServerContext * context, const TxControlRequest * request, ControlReply * reply) override;
    Status RxStart(ServerContext * context, const RxControlRequest * request, ControlReply * reply) override;
    Status TxStop(ServerContext * context, const StopControlRequest * request, ControlReply * reply) override;
    Status RxStop(ServerContext * context, const StopControlRequest * request, ControlReply * reply) override;
    Status Stop(ServerContext * context, const StopControlRequest * request, ControlReply * reply) override;

private:
    ProxyContext *m_ctx;
};

void RunRPCServer(ProxyContext *ctx);
