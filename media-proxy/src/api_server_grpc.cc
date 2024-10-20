/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "api_server_grpc.h"

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
    reply->set_status(controller::HealthCheckResponse_ServingStatus_SERVING);

    return Status::OK;
}

Status HealthServiceImpl::Watch(ServerContext* context, const HealthCheckRequest* request, HealthCheckResponse* reply)
{
    return Status::OK;
}

// DEBUG
#include "mediaproxy.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;
using mediaproxy::ControlAPI;
using mediaproxy::RegisterMediaProxyRequest;
using mediaproxy::RegisterMediaProxyReply;
using mediaproxy::RegisterConnectionRequest;
using mediaproxy::RegisterConnectionReply;
using mediaproxy::UnregisterConnectionRequest;
using mediaproxy::UnregisterConnectionReply;
using mediaproxy::StartCommandQueueRequest;
using mediaproxy::CommandMessage;

class ControlAPIClient {
 public:
  ControlAPIClient(std::shared_ptr<Channel> channel)
      : stub_(ControlAPI::NewStub(channel)) {}

  std::string RegisterMediaProxy() {
    RegisterMediaProxyRequest request;
    request.set_sdk_port(12345);

    RegisterMediaProxyReply reply;

    ClientContext context;

    Status status = stub_->RegisterMediaProxy(&context, request, &reply);

    if (status.ok()) {
      return reply.proxy_id();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RegisterMediaProxy rpc failed";
    }
  }

  std::string RegisterConnection(const std::string& proxy_id) {
    RegisterConnectionRequest request;
    request.set_proxy_id(proxy_id);
    request.set_kind(1);
    request.set_conn_type(2);
    request.set_payload_type(3);
    request.set_buffer_size(1024);

    RegisterConnectionReply reply;

    ClientContext context;

    Status status = stub_->RegisterConnection(&context, request, &reply);

    if (status.ok()) {
      return reply.conn_id();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RegisterConnection rpc failed";
    }
  }

  std::string UnregisterConnection(const std::string& proxy_id, const std::string& conn_id) {
    UnregisterConnectionRequest request;
    request.set_proxy_id(proxy_id);
    request.set_conn_id(conn_id);

    UnregisterConnectionReply reply;

    ClientContext context;

    Status status = stub_->UnregisterConnection(&context, request, &reply);

    if (status.ok()) {
      return "";
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "UnregisterConnection rpc failed";
    }
  }

  void StartCommandQueue(std::string& proxy_id) {
    CommandMessage reply;
    ClientContext context;

    StartCommandQueueRequest request;
    request.set_proxy_id(proxy_id);

    std::unique_ptr<ClientReader<CommandMessage> > reader(
        stub_->StartCommandQueue(&context, request));

    while (reader->Read(&reply)) {
      auto opcode = reply.opcode();

      if (!opcode.compare("create-multipoint-group")) {
        INFO("[AGENT CMD] Create multipoint group");
      } else if (!opcode.compare("delete-multipoint-group")) {
        INFO("[AGENT CMD] Delete multipoint group");
      } else if (!opcode.compare("create-bridge")) {
        INFO("[AGENT CMD] Create bridge");
      } else if (!opcode.compare("delete-bridge")) {
        INFO("[AGENT CMD] Delete bridge");
      } else {
        INFO("[AGENT CMD] Unknown opcode '%s', id '%s'", opcode.c_str(), reply.id().c_str());
      }
    }
    Status status = reader->Finish();
    if (status.ok()) {
      std::cout << "rpc succeeded." << std::endl;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "rpc failed." << std::endl;
    }
  }

 private:
  std::unique_ptr<ControlAPI::Stub> stub_;
};

ControlAPIClient *controlAPI;
std::string proxy_id;

static void * CommandQueueHandler(void *arg) {

    std::string& proxy_id = *(static_cast<std::string *>(arg));

    for (;;) {
        controlAPI->StartCommandQueue(proxy_id);
        sleep(1);
    }

    return NULL;
}
// DEBUG

void RunRPCServer(ProxyContext* ctx)
{
    ConfigureServiceImpl service(ctx);

    ServerBuilder builder;
    builder.AddListeningPort(ctx->getRPCListenAddress(), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    INFO("gRPC Server listening on %s", ctx->getRPCListenAddress().c_str());

    // DEBUG

    controlAPI = new ControlAPIClient(
        grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

    proxy_id = controlAPI->RegisterMediaProxy();

    pthread_t cq;
    int err;

    err = pthread_create(&cq, NULL, CommandQueueHandler, &proxy_id);
    if (err)
        ERROR("Command Queue creation failed (%d)", err);

    server->Wait();

    err = pthread_cancel(cq);
    if (err && err != ESRCH)
        ERROR("Command Queue thread cancel failed (%d)", err);

    err = pthread_join(cq, NULL);
    if (err && err != ESRCH)
        ERROR("Command Queue thread join failed (%d)", err);

    // DEBUG
}

std::string gRPC_RegisterConnection()
{
    std::string conn_id = controlAPI->RegisterConnection(proxy_id);
    std::cout << "ControlAPI received: " << conn_id << std::endl;
    return conn_id;
}

void gRPC_UnregisterConnection(const std::string& conn_id)
{
    std::string reply = controlAPI->UnregisterConnection(proxy_id, conn_id);
    std::cout << "ControlAPI received: " << reply << std::endl;
}
