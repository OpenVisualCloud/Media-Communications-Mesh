/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROXY_API_H
#define PROXY_API_H

#include <grpcpp/grpcpp.h>
#include "mediaproxy.grpc.pb.h"
#include "metrics.h"
#include "concurrency.h"
#include "conn.h"

using mediaproxy::CommandReply;

namespace mesh {

using grpc::Channel;
using mediaproxy::ProxyAPI;

class ProxyAPIClient {
public:
    ProxyAPIClient(std::shared_ptr<Channel> channel)
        : stub_(ProxyAPI::NewStub(channel)) {}

    int RegisterConnection(std::string& conn_id, const std::string& kind,
                           const connection::Config& config,
                           const std::string& name, std::string& err);
    int UnregisterConnection(const std::string& conn_id);
    int SendMetrics(const std::vector<telemetry::Metric>& metrics);
    int StartCommandQueue(context::Context& ctx);
    int SendCommandReply(CommandReply& request);

    int Run(context::Context& ctx);
    void Shutdown();

protected:
    int RegisterMediaProxy();
    int UnregisterMediaProxy();

    friend int RunProxyAPIClient(context::Context& ctx);

private:
    std::unique_ptr<ProxyAPI::Stub> stub_;
    std::string _proxy_id;
    std::mutex _proxy_id_mx;
    std::jthread th;

    void SetProxyId(const std::string& id);
    std::string GetProxyId();
};

extern std::unique_ptr<ProxyAPIClient> proxyApiClient;

int RunProxyAPIClient(context::Context& ctx);

} // namespace mesh

#endif // PROXY_API_H
