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
#include "../../mcm-version.h"

using grpc::Channel;
// using grpc::ClientContext;
using grpc::Status;
using sdk::SDKAPI;
using sdk::CreateConnectionRequest;
using sdk::CreateConnectionResponse;
using sdk::DeleteConnectionRequest;
using sdk::DeleteConnectionResponse;
using sdk::ConnectionConfig;
using sdk::BufferPartition;
using sdk::BufferPartitions;
using sdk::ConnectionKind;
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

using namespace mesh;

class SDKAPIClient {
public:
    SDKAPIClient(std::shared_ptr<Channel> channel)
        : stub_(SDKAPI::NewStub(channel)) {}

    int CreateConnection(std::string& conn_id, mcm_conn_param *param,
                         memif_conn_param *memif_param) {
        if (!param || !memif_param)
            return -1;

        CreateConnectionRequest req;
        req.set_client_id(client_id);

        std::string param_str(reinterpret_cast<const char *>(param),
                              sizeof(mcm_conn_param));
        req.set_mcm_conn_param(param_str);

        CreateConnectionResponse resp;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(5));

        Status status = stub_->CreateConnection(&context, req, &resp);

        if (status.ok()) {
            client_id = resp.client_id();
            conn_id = resp.conn_id();

            int sz = resp.memif_conn_param().size();
            if (sz != sizeof(memif_conn_param)) {
                log::error("Param size (%d) not equal to memif_conn_param (%ld)",
                           sz, sizeof(memif_conn_param));
                return -1;
            }

            memcpy(memif_param, resp.memif_conn_param().data(), sz);

            return 0;
        } else {
            log::error("CreateConnection RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }
    }

    int CreateConnectionJson(std::string& conn_id, const ConnectionJsonConfig& cfg,
                             memif_conn_param *memif_param) {
        if (!memif_param)
            return -1;

        CreateConnectionRequest req;
        req.set_client_id(client_id);

        // mcm_conn_param param = {
        //     .type = is_rx,
        //     .local_addr = {
        //         .port = "9001",
        //     },
        //     .remote_addr = {
        //         .ip = "192.168.96.10",
        //     },
        //     .width = 640,
        //     .height = 360,
        //     .fps = 60,
        //     .pix_fmt = PIX_FMT_YUV422P_10BIT_LE,
        // };
        // std::string param_str(reinterpret_cast<const char *>(&param),
        //                       sizeof(mcm_conn_param));
        // req.set_mcm_conn_param(param_str);

        auto config = req.mutable_config();
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
            st2110->set_remote_ip_addr(cfg.conn.st2110.remote_ip_addr);
            st2110->set_remote_port(cfg.conn.st2110.remote_port);
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

        CreateConnectionResponse resp;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(20));

        Status status = stub_->CreateConnection(&context, req, &resp);

        if (status.ok()) {
            client_id = resp.client_id();
            conn_id = resp.conn_id();

            int sz = resp.memif_conn_param().size();
            if (sz != sizeof(memif_conn_param)) {
                log::error("Param size (%d) not equal to memif_conn_param (%ld)",
                           sz, sizeof(memif_conn_param));
                return -1;
            }

            memcpy(memif_param, resp.memif_conn_param().data(), sz);

            return 0;
        } else {
            log::error("CreateConnectionJson RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }
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

        if (status.ok()) {
            return 0;
        } else {
            log::error("DeleteConnection RPC failed: %s",
                       status.error_message().c_str());
            return -1;
        }
    }

    std::string client_id;

private:
    std::unique_ptr<SDKAPI::Stub> stub_;
};

// DEBUG
extern "C" int get_media_proxy_addr(mcm_dp_addr* proxy_addr);
// DEBUG

void * mesh_grpc_create_client()
{
    // DEBUG
    std::string sdk_port("50050");

    mcm_dp_addr media_proxy_addr = {};
    auto err = get_media_proxy_addr(&media_proxy_addr);
    if (!err) {
        sdk_port = std::string(media_proxy_addr.port);
    }
    // DEBUG

    auto client = new(std::nothrow)
        SDKAPIClient(grpc::CreateChannel("localhost:" + sdk_port, // gRPC default 50051
                     grpc::InsecureChannelCredentials()));
    return client;
}

void * mesh_grpc_create_client_json(const std::string& endpoint) {

    log::info("Media Communications Mesh SDK version %s #%s", VERSION_TAG, VERSION_HASH)
             ("endpoint", endpoint);

    auto client = new(std::nothrow)
        SDKAPIClient(grpc::CreateChannel(endpoint,
                     grpc::InsecureChannelCredentials()));
    return client;
}

void mesh_grpc_destroy_client(void *client)
{
    if (client) {
        auto cli = static_cast<SDKAPIClient *>(client);
        delete cli;
    }
}

class GrpcConn {
public:
    // This declaration must go first to allow proper type casting.
    mcm_conn_context *handle;

    SDKAPIClient *client;
    std::string conn_id;
};

// Can't include the entire header file due to the C/C++ atomics incompatibility.
extern "C"
mcm_conn_context* mcm_create_connection_memif(mcm_conn_param* svc_args,
                                              memif_conn_param* memif_args);

void * mesh_grpc_create_conn(void *client, mcm_conn_param *param)
{
    if (!client)
        return NULL;

    auto cli = static_cast<SDKAPIClient *>(client);

    auto conn = new(std::nothrow) GrpcConn();
    if (!conn)
        return NULL;

    memif_conn_param memif_param = {};

    int err = cli->CreateConnection(conn->conn_id, param, &memif_param);
    if (err) {
        delete conn;
        log::error("Create gRPC connection failed (%d)", err);
        return NULL;
    }

    log::info("gRPC: connection created")("id", conn->conn_id)
                                         ("client_id", cli->client_id);

    conn->client = cli;

    // Connect memif connection
    // TODO: Propagate the main context to enable cancellation.
    conn->handle = mcm_create_connection_memif(param, &memif_param);
    if (!conn->handle) {
        delete conn;
        log::error("gRPC: failed to create memif interface");
        return NULL;
    }

    return conn;
}

void * mesh_grpc_create_conn_json(void *client, const mesh::ConnectionJsonConfig& cfg)
{
    if (!client)
        return NULL;

    auto cli = static_cast<SDKAPIClient *>(client);

    auto conn = new(std::nothrow) GrpcConn();
    if (!conn)
        return NULL;

    memif_conn_param memif_param = {};

    int err = cli->CreateConnectionJson(conn->conn_id, cfg, &memif_param);
    if (err) {
        delete conn;
        log::error("Create gRPC connection failed (%d)", err);
        return NULL;
    }

    log::info("gRPC: connection created")("id", conn->conn_id)
                                         ("client_id", cli->client_id);

    conn->client = cli;

    // DEBUG
    // mcm_conn_param param = {
    //     .type = is_rx,
    //     .width = 640,
    //     .height = 360,
    //     .fps = 60,
    //     .pix_fmt = PIX_FMT_YUV422P_10BIT_LE,
    // };
    // DEBUG
    // mcm_conn_param param = {
    //     .type = cfg.kind == MESH_CONN_KIND_SENDER ? is_tx : is_rx,
    //     .width = cfg.payload.video.width,
    //     .height = cfg.payload.video.height,
    //     .fps = cfg.payload.video.fps,
    //     .pix_fmt = PIX_FMT_YUV422P_10BIT_LE,
    // };
    // DEBUG

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

void mesh_grpc_destroy_conn(void *conn_ptr)
{
    if (!conn_ptr)
        return;

    auto conn = static_cast<GrpcConn *>(conn_ptr);

    int err = conn->client->DeleteConnection(conn->conn_id);
    if (err)
        log::error("Delete gRPC connection failed (%d)", err);

    log::info("gRPC: connection deleted")("id", conn->conn_id);

    delete conn;
}

