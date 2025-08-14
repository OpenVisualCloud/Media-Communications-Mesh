/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mesh_sdk_api.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include <grpcpp/grpcpp.h>
#include "sdk.grpc.pb.h"
#include "mesh_logger.h"
#include "mcm-version.h"
#include "mesh_dp_legacy.h"
#include "concurrency.h"
#include "mesh_conn_zc.h"

using grpc::Channel;
using grpc::Status;
using sdk::SDKAPI;
using sdk::CreateConnectionRequest;
using sdk::CreateConnectionResponse;
using sdk::CreateConnectionZeroCopyResponse;
using sdk::ActivateConnectionRequest;
using sdk::ActivateConnectionResponse;
using sdk::DeleteConnectionRequest;
using sdk::DeleteConnectionResponse;
using sdk::RegisterRequest;
using sdk::Event;
using sdk::SendMetricsRequest;
using sdk::ConnectionMetrics;
using sdk::SendMetricsResponse;
using sdk::BufferPartition;
using sdk::BufferPartitions;
using sdk::ConnectionKind;
using sdk::ConnectionConfig;
using sdk::ConfigMultipointGroup;
using sdk::ConfigST2110;
using sdk::ConfigRDMA;
using sdk::ConfigVideo;
using sdk::ConfigAudio;
using sdk::ConfigBlob;
using sdk::ST2110Transport;
using sdk::VideoPixelFormat;
using sdk::AudioSampleRate;
using sdk::AudioFormat;
using sdk::AudioPacketTime;

extern "C" void mcm_cancel_poll_event_memif(void *pctx);

using namespace mesh;

class SDKAPIClient;

class ProxyConn {
public:
    // This declaration must go first to allow proper type casting.
    mcm_conn_context *handle;

    SDKAPIClient *client;
    std::string conn_id;
};

class SDKAPIClient {
private:
    void assign_pb_from_conn_cfg(sdk::ConnectionConfig *config,
                                 const mesh::ConnectionConfig& cfg) {
        if (!config)
            return;

        config->set_buf_queue_capacity(cfg.buf_queue_capacity);
        config->set_max_payload_size(cfg.max_payload_size);
        config->set_max_metadata_size(cfg.max_metadata_size);
        config->set_calculated_payload_size(cfg.calculated_payload_size);

        auto buf_parts = config->mutable_buf_parts();

        auto buf_part_payload = buf_parts->mutable_payload();
        buf_part_payload->set_offset(cfg.buf_parts.payload.offset);
        buf_part_payload->set_size(cfg.buf_parts.payload.size);

        auto buf_part_metadata = buf_parts->mutable_metadata();
        buf_part_metadata->set_offset(cfg.buf_parts.metadata.offset);
        buf_part_metadata->set_size(cfg.buf_parts.metadata.size);

        auto buf_part_sysdata = buf_parts->mutable_sysdata();
        buf_part_sysdata->set_offset(cfg.buf_parts.sysdata.offset);
        buf_part_sysdata->set_size(cfg.buf_parts.sysdata.size);

        config->set_kind((ConnectionKind)cfg.kind);

        if (cfg.conn_type == MESH_CONN_TYPE_GROUP) {
            auto group = new ConfigMultipointGroup();
            group->set_urn(cfg.conn.multipoint_group.urn);
            config->set_allocated_multipoint_group(group);
        } else if (cfg.conn_type == MESH_CONN_TYPE_ST2110) {
            auto st2110 = new ConfigST2110();
            st2110->set_ip_addr(cfg.conn.st2110.ip_addr);
            st2110->set_port(cfg.conn.st2110.port);
            st2110->set_mcast_sip_addr(cfg.conn.st2110.mcast_sip_addr);
            st2110->set_transport((ST2110Transport)cfg.conn.st2110.transport);
            st2110->set_pacing(cfg.conn.st2110.pacing);
            st2110->set_payload_type(cfg.conn.st2110.payload_type);
            config->set_allocated_st2110(st2110);
        } else if (cfg.conn_type == MESH_CONN_TYPE_RDMA) {
            auto rdma = new ConfigRDMA();
            rdma->set_connection_mode(cfg.conn.rdma.connection_mode);
            rdma->set_max_latency_ns(cfg.conn.rdma.max_latency_ns);
            config->set_allocated_rdma(rdma);
        }

        auto options = config->mutable_options();
        options->set_engine(cfg.options.engine);

        auto options_rdma = options->mutable_rdma();
        options_rdma->set_provider(cfg.options.rdma.provider);
        options_rdma->set_num_endpoints(cfg.options.rdma.num_endpoints);

        if (cfg.payload_type == MESH_PAYLOAD_TYPE_VIDEO) {
            auto video = new ConfigVideo();
            video->set_width(cfg.payload.video.width);
            video->set_height(cfg.payload.video.height);
            video->set_fps(cfg.payload.video.fps);
            video->set_pixel_format((VideoPixelFormat)cfg.payload.video.pixel_format);
            config->set_allocated_video(video);
        } else if (cfg.payload_type == MESH_PAYLOAD_TYPE_AUDIO) {
            auto audio = new ConfigAudio();
            audio->set_channels(cfg.payload.audio.channels);
            audio->set_sample_rate((AudioSampleRate)cfg.payload.audio.sample_rate);
            audio->set_format((AudioFormat)cfg.payload.audio.format);
            audio->set_packet_time((AudioPacketTime)cfg.payload.audio.packet_time);
            config->set_allocated_audio(audio);
        } else if (cfg.payload_type == MESH_PAYLOAD_TYPE_BLOB) {
            auto blob = new ConfigBlob();
            config->set_allocated_blob(blob);
        }
    }

public:
    SDKAPIClient(std::shared_ptr<Channel> channel, mesh::ClientContext *parent)
        : stub_(SDKAPI::NewStub(channel)), parent(parent) {}

    int CreateConnection(std::string& conn_id, const mesh::ConnectionConfig& cfg,
                         memif_conn_param *memif_param) {
        if (!memif_param)
            return -1;

        CreateConnectionRequest req;
        req.set_client_id(client_id);
        req.set_name(cfg.name);

        auto config = req.mutable_config();
        assign_pb_from_conn_cfg(config, cfg);

        CreateConnectionResponse resp;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(20));

        Status status = stub_->CreateConnection(&context, req, &resp);
        if (!status.ok()) {
            log::error("CreateConnection RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }

        conn_id = resp.conn_id();

        int sz = resp.memif_conn_param().size();
        if (sz != sizeof(memif_conn_param)) {
            log::error("Param size (%d) not equal to memif_conn_param (%ld)",
                        sz, sizeof(memif_conn_param));
            return -1;
        }

        memcpy(memif_param, resp.memif_conn_param().data(), sz);

        return 0;
    }

    int CreateConnectionZeroCopy(std::string& conn_id, const mesh::ConnectionConfig& cfg,
                                 const std::string& temporary_id) {

        CreateConnectionRequest req;
        req.set_client_id(client_id);
        req.set_name(cfg.name);
        req.set_temporary_id(temporary_id);

        auto config = req.mutable_config();
        assign_pb_from_conn_cfg(config, cfg);

        CreateConnectionZeroCopyResponse resp;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(20));

        Status status = stub_->CreateConnectionZeroCopy(&context, req, &resp);
        if (!status.ok()) {
            log::error("CreateConnectionZeroCopy RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }

        conn_id = resp.conn_id();

        return 0;
    }

    int ActivateConnection(std::string& conn_id) {
        ActivateConnectionRequest req;
        req.set_client_id(client_id);
        req.set_conn_id(conn_id);

        ActivateConnectionResponse resp;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(20));

        Status status = stub_->ActivateConnection(&context, req, &resp);

        if (!status.ok()) {
            log::error("ActivateConnection RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }

        if (!resp.linked())
            return -EAGAIN;

        return 0;
    }

    int DeleteConnection(const std::string& conn_id) {
        DeleteConnectionRequest req;
        req.set_client_id(client_id);
        req.set_conn_id(conn_id);

        DeleteConnectionResponse resp;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(5));

        Status status = stub_->DeleteConnection(&context, req, &resp);

        if (!status.ok()) {
            log::error("DeleteConnection RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }

        return 0;
    }

    void ReportMetrics() {
        SendMetricsRequest req;
        req.set_client_id(client_id);

        if (parent) {
            std::lock_guard<std::mutex> lk(parent->mx);
            for (const auto conn : parent->conns) {
                if (!dynamic_cast<ZeroCopyConnectionContext *>(conn))
                    continue;

                auto proxy_conn = static_cast<ProxyConn *>(conn->proxy_conn);
                if (!proxy_conn)
                    continue;

                ConnectionMetrics *metric = req.add_conn_metrics();
                metric->set_conn_id(proxy_conn->conn_id);
                metric->set_inbound_bytes(conn->metrics.inbound_bytes);
                metric->set_outbound_bytes(conn->metrics.outbound_bytes);
                metric->set_transactions_succeeded(conn->metrics.transactions_succeeded);
                metric->set_transactions_failed(conn->metrics.transactions_failed);
                metric->set_errors(conn->metrics.errors);
            }
        }

        SendMetricsResponse resp;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(5));

        Status status = stub_->SendMetrics(&context, req, &resp);

        if (!status.ok()) {
            log::error("SendMetrics RPC failed: %s",
                       status.error_message().c_str());
        }
    }

    void RegisterAndStreamEvents() {
        RegisterRequest req;
    
        grpc::ClientContext context;

        thread::Channel<bool> report_metrics_ch;

        std::jthread aux_th([&]() {
            while (!ctx.cancelled()) {
                auto v = report_metrics_ch.receive(ctx);
                if (!v.has_value())
                    continue;

                if (v.value())
                    ReportMetrics();
            }
            context.TryCancel();
        });

        thread::Defer d([&]{ ctx.cancel(); });

        std::unique_ptr<grpc::ClientReader<Event>> reader(
            stub_->RegisterAndStreamEvents(&context, req));

        Event event;
        while (reader->Read(&event)) {
            if (event.has_client_registered()) {
                client_id = event.client_registered().client_id();
                registered_ch.send(ctx, true);
            } else if (event.has_conn_unlink_requested()) {
                auto conn_id = event.conn_unlink_requested().conn_id();
                log::debug("[EVENT] Conn unlink requested")("id", conn_id);

                if (parent) {
                    std::lock_guard<std::mutex> lk(parent->mx);
                    for (const auto conn : parent->conns) {
                        // TODO: close by conn_id, not all connections!!!

                        conn->ctx.cancel();
                        if (conn->proxy_conn) {
                            auto handle = *(mcm_conn_context **)conn->proxy_conn;
                            if (handle)
                                mcm_cancel_poll_event_memif(handle->priv);
                        }
                    }
                }
            } else if (event.has_conn_zero_copy_config()) {
                auto zc_config = event.conn_zero_copy_config();
                auto conn_id = zc_config.conn_id();
                auto temporary_id = zc_config.temporary_id();
                auto sysv_key = zc_config.sysv_key();
                auto mem_region_sz = zc_config.mem_region_sz();
                log::debug("[EVENT] Conn ZC config")("id", conn_id)
                                                    ("temporary_id", temporary_id)
                                                    ("sysv_key", sysv_key)
                                                    ("mem_region_sz", mem_region_sz);

                if (parent) {
                    std::lock_guard<std::mutex> lk(parent->mx);
                    for (const auto conn : parent->conns) {
                        auto zc_conn = dynamic_cast<ZeroCopyConnectionContext *>(conn);
                        if (zc_conn && !zc_conn->temporary_id.compare(temporary_id)) {
                            auto zc_cfg = zerocopy::Config{
                                .sysv_key = sysv_key,
                                .mem_region_sz = mem_region_sz,
                            };
                            zc_conn->zero_copy_config_ch.send(ctx, zc_cfg);
                            break;
                        }
                    }
                }
            } else if (event.has_report_metrics_triggered()) {
                report_metrics_ch.try_send(true);
            } else {
                log::info("Received unknown event type");
            }
        }

        Status status = reader->Finish();
        registered_ch.send(ctx, false);
        if (!status.ok()) {
            if (status.error_code() == grpc::StatusCode::CANCELLED)
                return;
        
            log::error("RegisterAndStreamEvents RPC failed: %s",
                       status.error_message().c_str());
        }
    }

    int Run() {
        try {
            th = std::jthread([this]() {
                RegisterAndStreamEvents();
            });
        }
        catch (const std::system_error& e) {
            log::error("SDK client background thread creation failed");
            Shutdown();
            return -ENOMEM;
        }

        auto tctx = context::WithTimeout(gctx, std::chrono::milliseconds(15000));
        auto registered = registered_ch.receive(tctx);
        if (!registered.has_value()) {
            if (tctx.cancelled() && !gctx.cancelled())
                log::error("SDK client registration timeout");
            Shutdown();
            return -EIO;
        }
        if (!registered.value()) {
            log::error("SDK client registration failed");
            Shutdown();
            return -EIO;
        }

        log::info("SDK client registered successfully")("client_id", client_id);

        return 0;
    }

    void Shutdown() {
        ctx.cancel();
        if (th.joinable())
            th.join();
    }

    std::string client_id;

private:
    std::unique_ptr<SDKAPI::Stub> stub_;
    std::jthread th;
    thread::Channel<bool> registered_ch;
    context::Context ctx = context::WithCancel(context::Background());
    mesh::ClientContext *parent = nullptr;
};

namespace mesh {

void * create_proxy_client(const std::string& endpoint, mesh::ClientContext *parent)
{
    log::info("Media Communications Mesh SDK version %s #%s", VERSION_TAG, VERSION_HASH)
             ("endpoint", endpoint);

    auto client = new(std::nothrow)
        SDKAPIClient(grpc::CreateChannel(endpoint,
                     grpc::InsecureChannelCredentials()), parent);

    if (client) {
        auto err = client->Run();
        if (err) {
            delete client;
            return NULL;
        }
    }

    return client;
}

void destroy_proxy_client(void *client)
{
    if (client) {
        auto cli = static_cast<SDKAPIClient *>(client);
        cli->Shutdown();
        delete cli;
    }
}

// Can't include the entire header file due to the C/C++ atomics incompatibility.
extern "C"
mcm_conn_context* mcm_create_connection_memif(mcm_conn_param* svc_args,
                                              memif_conn_param* memif_args);

void * create_proxy_conn(void *client, const mesh::ConnectionConfig& cfg)
{
    if (!client)
        return NULL;

    auto cli = static_cast<SDKAPIClient *>(client);

    auto conn = new(std::nothrow) ProxyConn();
    if (!conn)
        return NULL;

    memif_conn_param memif_param = {};

    int err = cli->CreateConnection(conn->conn_id, cfg, &memif_param);
    if (err) {
        delete conn;
        log::error("Create gRPC connection failed (%d)", err);
        return NULL;
    }

    log::info("gRPC: connection created")("id", conn->conn_id)
                                         ("client_id", cli->client_id);

    conn->client = cli;

    mcm_conn_param param = {
        .type = cfg.kind == MESH_CONN_KIND_SENDER ? is_tx : is_rx,
    };
    cfg.assign_to_mcm_conn_param(param);

    // Connect memif connection
    // TODO: Propagate the main context to enable cancellation.
    conn->handle = mcm_create_connection_memif(&param, &memif_param);
    if (!conn->handle) {
        delete conn;
        log::error("gRPC: failed to create memif interface");
        return NULL;
    }

    while (!gctx.cancelled()) {
        err = cli->ActivateConnection(conn->conn_id);
        if (err == -EAGAIN) {
            // Repeat after a small delay
            thread::Sleep(gctx, std::chrono::milliseconds(50));
            continue;
        }
        break;
    }

    if (err) {
        log::error("Activate gRPC connection failed (%d)", err);
        destroy_proxy_conn(conn);
        return NULL;
    }

    log::info("gRPC: connection active")("id", conn->conn_id)
                                        ("client_id", cli->client_id);

    // Workaround to allow Mesh Agent and Media Proxies to apply necessary
    // configuration after registering the connection. The delay should
    // be sufficient for all Media Proxies to complete creating multipoint
    // groups and bridges before the user app starts sending data. This WA
    // should prevent first frame losses in 95 percent cases.
    if (cfg.kind == MESH_CONN_KIND_SENDER && cfg.tx_conn_creation_delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.tx_conn_creation_delay));
    }

    return conn;
}

void destroy_proxy_conn(void *conn_ptr)
{
    if (!conn_ptr)
        return;

    auto conn = static_cast<ProxyConn *>(conn_ptr);

    int err = conn->client->DeleteConnection(conn->conn_id);
    if (err)
        log::error("Delete gRPC connection failed (%d)", err);

    log::info("gRPC: connection deleted")("id", conn->conn_id);

    delete conn;
}

void * create_proxy_conn_zero_copy(void *client, const mesh::ConnectionConfig& cfg,
                                   const std::string& temporary_id)
{
    if (!client)
        return NULL;

    auto cli = static_cast<SDKAPIClient *>(client);

    auto conn = new(std::nothrow) ProxyConn();
    if (!conn)
        return NULL;

    int err = cli->CreateConnectionZeroCopy(conn->conn_id, cfg, temporary_id);
    if (err) {
        delete conn;
        log::error("Create gRPC connection ZC failed (%d)", err);
        return NULL;
    }

    log::info("gRPC: ZC connection created")("id", conn->conn_id)
                                            ("temporary_id", temporary_id)
                                            ("client_id", cli->client_id);

    conn->client = cli;

    return conn;
}

int configure_proxy_conn_zero_copy(void *conn_ptr)
{
    if (!conn_ptr)
        return -EINVAL;

    auto conn = static_cast<ZeroCopyConnectionContext *>(conn_ptr);
    auto proxy_conn = static_cast<ProxyConn *>(conn->proxy_conn);

    log::debug("CONFIG ZC")("conn_id", proxy_conn->conn_id);

    // TODO: take timeout interval from the client configuration.
    auto tctx = context::WithTimeout(gctx, std::chrono::milliseconds(15000));
    auto config = conn->zero_copy_config_ch.receive(tctx);
    if (!config.has_value()) {
        if (tctx.cancelled() && !gctx.cancelled())
            log::error("SDK conn ZC config timeout");
        return -EIO;
    }

    conn->zc_config.sysv_key = config.value().sysv_key;
    conn->zc_config.mem_region_sz = config.value().mem_region_sz;

    log::debug("SDK conn ZC config received")
              ("sysv_key", conn->zc_config.sysv_key)
              ("mem_region_sz", conn->zc_config.mem_region_sz);

    if (conn->cfg.kind == MESH_CONN_KIND_RECEIVER) {
        auto ret = conn->gw.init(gctx, &conn->zc_config);
        if (ret != zerocopy::gateway::Result::success)
            return -1;

        conn->gw.set_tx_callback([conn](context::Context& ctx, void *ptr, uint32_t sz,
                                uint32_t& sent) -> zerocopy::gateway::Result {
            ZeroCopyRxEvent evt = {
                .ptr = ptr,
                .sz = sz,
                .err = 0,
            };

            auto res = conn->zero_copy_rx_ch.try_send(std::move(evt));

            return zerocopy::gateway::Result::success;
        });
    } else {
        auto ret = conn->gw_rx.init(gctx, &conn->zc_config);
        if (ret != zerocopy::gateway::Result::success)
            return -1;
    }

    int err = 0;
    auto cli = static_cast<SDKAPIClient *>(proxy_conn->client);

    while (!gctx.cancelled()) {
        err = cli->ActivateConnection(proxy_conn->conn_id);
        if (err == -EAGAIN) {
            // Repeat after a small delay
            thread::Sleep(gctx, std::chrono::milliseconds(50));
            continue;
        }
        break;
    }

    if (err) {
        log::error("Activate gRPC ZC connection failed (%d)", err);
        destroy_proxy_conn_zero_copy(conn);
        return err;
    }

    log::info("gRPC: connection active")("id", proxy_conn->conn_id)
                                        ("client_id", cli->client_id);

    return 0;
}

void destroy_proxy_conn_zero_copy(void *conn_ptr)
{
    if (!conn_ptr)
        return;

    auto conn = static_cast<ZeroCopyConnectionContext *>(conn_ptr);
    auto proxy_conn = static_cast<ProxyConn *>(conn->proxy_conn);

    int err = proxy_conn->client->DeleteConnection(proxy_conn->conn_id);
    if (err)
        log::error("Delete gRPC ZC connection failed (%d)", err);

    log::info("gRPC: ZC connection deleted")("id", proxy_conn->conn_id);

    if (conn->cfg.kind == MESH_CONN_KIND_RECEIVER) {
        conn->gw.shutdown(gctx);
    } else {
        conn->gw_rx.shutdown(gctx);
    }

    delete proxy_conn;
}

} // namespace mesh
