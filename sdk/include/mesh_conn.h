/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_CONN_H
#define MESH_CONN_H

#include <string>
#include "mesh_dp.h"
#include "mcm_dp.h"
#include "mesh_client.h"
#include "mesh_buf.h"

namespace mesh {
class ConnectionConfig;
} // namespace mesh

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct mesh_internal_ops_t {
    mcm_conn_context * (*create_conn)(mcm_conn_param *param);
    void (*destroy_conn)(mcm_conn_context *pctx);
    mcm_buffer * (*dequeue_buf)(mcm_conn_context *pctx, int timeout, int *error_code);
    int (*enqueue_buf)(mcm_conn_context *pctx, mcm_buffer *buf);

    void * (*grpc_create_client)();
    void * (*grpc_create_client_json)(const std::string& endpoint,
                                      mesh::ClientContext *parent);
    void (*grpc_destroy_client)(void *client);
    void * (*grpc_create_conn)(void *client, mcm_conn_param *param);
    void * (*grpc_create_conn_json)(void *client, const mesh::ConnectionConfig& cfg);
    void (*grpc_destroy_conn)(void *conn);
};

extern struct mesh_internal_ops_t mesh_internal_ops;

/**
 * Mesh connection type constants.
 */
#define MESH_CONN_TYPE_UNINITIALIZED -1 ///< Connection type is uninitialized
#define MESH_CONN_TYPE_MEMIF          0 ///< Single node direct connection via memif
#define MESH_CONN_TYPE_GROUP          1 ///< Local connection to Multipoint Group
#define MESH_CONN_TYPE_ST2110         2 ///< SMPTE ST2110-xx connection via Media Proxy
#define MESH_CONN_TYPE_RDMA           3 ///< RDMA connection via Media Proxy

/**
 * Payload type constants.
 */
#define MESH_PAYLOAD_TYPE_UNINITIALIZED -1 ///< Payload type is uninitialized
#define MESH_PAYLOAD_TYPE_BLOB           0 ///< Payload: blob arbitrary data
#define MESH_PAYLOAD_TYPE_VIDEO          1 ///< Payload: video frames
#define MESH_PAYLOAD_TYPE_AUDIO          2 ///< Payload: audio packets

namespace mesh {

class ConnectionConfig {
public:
    int parse_from_json(const char *str);
    int calc_payload_size();
    int configure_buf_partitions();
    int assign_to_mcm_conn_param(mcm_conn_param& param) const;

    // Connection kind (transmitter, receiver).
    // Any value of the MESH_CONN_KIND_* constants.
    int kind;

    uint16_t buf_queue_capacity;
    uint32_t max_payload_size;
    uint32_t max_metadata_size;

    uint32_t calculated_payload_size;

    BufferPartitions buf_parts;

    uint16_t tx_conn_creation_delay;

    // Connection type (Multipoint Group, SMPTE ST2110-XX, RDMA).
    // Any value of the MESH_CONN_TYPE_* constants.
    int conn_type = MESH_CONN_TYPE_UNINITIALIZED;

    struct {
        struct {
            // Unified Resource Name of Multipoint Group.
            // Example: "ipv4:224.0.0.1:9003".
            std::string urn;
        } multipoint_group;

        struct {
            std::string ip_addr;
            uint16_t port;
            std::string mcast_sip_addr;

            // SMPTE ST2110-xx transport type.
            // Any value of the MESH_CONN_TRANSPORT_ST2110_* constants.
            int transport;

            std::string pacing;
            uint8_t payload_type;
            std::string transportPixelFormat;
        } st2110;

        struct {
            std::string connection_mode;
            uint32_t max_latency_ns;
        } rdma;
    } conn;

    struct {
        struct {
            std::string provider = "tcp";
            uint8_t num_endpoints = 1;
        } rdma;
    } options;

    // Payload type (Video, Audio).
    // Any value of the MESH_PAYLOAD_TYPE_* constants.
    int payload_type = MESH_PAYLOAD_TYPE_UNINITIALIZED;

    struct {
        struct {
            uint32_t width;
            uint32_t height;
            double fps;

            // Video frame pixel format.
            // Any value of the MESH_VIDEO_PIXEL_FORMAT_* constants.
            int pixel_format;
        } video;

        struct {
            int channels;

            // Audio sample rate.
            // Any value of the MESH_AUDIO_SAMPLE_RATE_* constants.
            int sample_rate;

            // Audio sample format.
            // Any value of the MESH_AUDIO_FORMAT_* constants.
            int format;

            // Audio packet time.
            // Any value of the MESH_AUDIO_PACKET_TIME_* constants.
            int packet_time;
        } audio;

    } payload;

private:
    int calc_audio_buf_size();
    int calc_video_buf_size();
};

/**
 * Mesh connection context structure
 */
class ConnectionContext {
public:
    explicit ConnectionContext(ClientContext *parent);
    ~ConnectionContext();

    int apply_json_config(const char *config);
    int establish();
    int shutdown();
    int get_buffer_timeout(MeshBuffer **buf, int timeout_ms);

    /**
     * NOTE: The __public structure is directly mapped in the memory to the
     * MeshConnection structure, which is publicly accessible to the user.
     * Therefore, the __public structure _MUST_ be placed first here.
     */
    MeshConnection __public = {};

    /**
     * NOTE: All declarations below this point are hidden from the user.
     */

    /**
     * MCM connection handle.
     */
    mcm_conn_context *handle = nullptr;

    void *grpc_conn = nullptr;

    ConnectionConfig cfg;

    context::Context ctx = context::WithCancel(context::Background());
};

} // namespace mesh

#endif // MESH_CONN_H
