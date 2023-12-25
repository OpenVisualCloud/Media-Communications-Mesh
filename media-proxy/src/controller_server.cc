/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "controller_server.h"

ConfigureServiceImpl::ConfigureServiceImpl(ProxyContext* ctx)
    : m_ctx(ctx)
{
}

Status ConfigureServiceImpl::TxStart(ServerContext* context, const TxControlRequest* request, ControlReply* reply)
{
    int session_id = 0;
    std::string ret_msg = "";

    std::cout << "\nReceived command: TxStart." << std::endl;

    session_id = m_ctx->TxStart(request);
    if (session_id >= 0) {
        ret_msg = std::to_string(session_id);
    } else {
        ret_msg = "Failed";
    }
    reply->set_message("Create MTL TX session: " + ret_msg);

    return Status::OK;
}

Status ConfigureServiceImpl::RxStart(ServerContext* context, const RxControlRequest* request, ControlReply* reply)
{
    int session_id = 0;
    std::string ret_msg = "";

    std::cout << "\nReceived command: RxStart." << std::endl;

    session_id = m_ctx->RxStart(request);
    if (session_id >= 0) {
        ret_msg = std::to_string(session_id);
    } else {
        ret_msg = "Failed";
    }
    reply->set_message("Create MTL RX session: " + ret_msg);

    return Status::OK;
}

Status ConfigureServiceImpl::TxStop(ServerContext* context, const StopControlRequest* request, ControlReply* reply)
{
    std::cout << "\nReceived command: Stop." << std::endl;

    m_ctx->TxStop(request->session_id());
    reply->set_message("gPRC reply: well received.");

    return Status::OK;
}

Status ConfigureServiceImpl::RxStop(ServerContext* context, const StopControlRequest* request, ControlReply* reply)
{
    std::cout << "\nReceived command: Stop." << std::endl;

    m_ctx->RxStop(request->session_id());
    reply->set_message("gPRC reply: well received.");

    return Status::OK;
}

Status ConfigureServiceImpl::Stop(ServerContext* context, const StopControlRequest* request, ControlReply* reply)
{
    std::cout << "\nReceived command: Stop." << std::endl;

    m_ctx->Stop();
    reply->set_message("gPRC reply: well received.");

    return Status::OK;
}

MsmDataPlaneServiceImpl::MsmDataPlaneServiceImpl(ProxyContext* ctx)
    : m_ctx(ctx)
{
}

Status MsmDataPlaneServiceImpl::stream_add_del(ServerContext* context, const StreamData* request, StreamResult* reply)
{
    return Status::OK;
}

HealthServiceImpl::HealthServiceImpl(ProxyContext* ctx)
    : m_ctx(ctx)
{
}

Status HealthServiceImpl::Check(ServerContext* context, const HealthCheckRequest* request, HealthCheckResponse* reply)
{
    return Status::OK;
}

Status HealthServiceImpl::Watch(ServerContext* context, const HealthCheckRequest* request, HealthCheckResponse* reply)
{
    return Status::OK;
}

void RunRPCServer(ProxyContext* ctx)
{
    ConfigureServiceImpl service(ctx);

    ServerBuilder builder;
    builder.AddListeningPort(ctx->getRPCListenAddress(), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    INFO("gRPC Server listening on %s", ctx->getRPCListenAddress().c_str());

    server->Wait();
}
