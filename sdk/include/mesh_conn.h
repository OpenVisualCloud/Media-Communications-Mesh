/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_CONN_H
#define MESH_CONN_H

#include "mesh_dp.h"
#include "mcm_dp.h"
#include "mesh_client.h"

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct mesh_internal_ops_t {
    mcm_conn_context * (*create_conn)(mcm_conn_param *param);
    void (*destroy_conn)(mcm_conn_context *pctx);
    mcm_buffer * (*dequeue_buf)(mcm_conn_context *pctx, int timeout, int *error_code);
    int (*enqueue_buf)(mcm_conn_context *pctx, mcm_buffer *buf);

    void * (*grpc_create_client)();
    void (*grpc_destroy_client)(void *client);
    void * (*grpc_create_conn)(void *client, mcm_conn_param *param);
    void (*grpc_destroy_conn)(void *conn);
};

extern struct mesh_internal_ops_t mesh_internal_ops;

namespace mesh {

/**
 * Mesh connection context structure
 */
class ConnectionContext {
public:
    ConnectionContext(ClientContext *parent);
    ~ConnectionContext();

    int apply_config_memif(MeshConfig_Memif *config);
    int apply_config_st2110(MeshConfig_ST2110 *config);
    int apply_config_rdma(MeshConfig_RDMA *config);

    int apply_config_video(MeshConfig_Video *config);
    int apply_config_audio(MeshConfig_Audio *config);

    int parse_payload_config(mcm_conn_param *param);
    int parse_conn_config(mcm_conn_param *param);

    int establish(int kind);
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

    /**
     * Configuration structure
     */
    struct {
        /**
         * Connection kind (sender, receiver).
         * Any value of the MESH_CONN_KIND_* constants.
         */
        int kind;

        /**
         * Connection type (memif, SMPTE ST2110-XX, RDMA).
         * Any value of the MESH_CONN_TYPE_* constants.
         */
        int conn_type;
#define MESH_CONN_TYPE_MEMIF  0 ///< Single node direct connection via memif
#define MESH_CONN_TYPE_ST2110 1 ///< SMPTE ST2110-xx connection via Media Proxy
#define MESH_CONN_TYPE_RDMA   2 ///< RDMA connection via Media Proxy

        /**
         * Configuration structures of all connection types
         */
        union {
            MeshConfig_Memif memif;
            MeshConfig_ST2110 st2110;
            MeshConfig_RDMA rdma;
        } conn;

        /**
         * Payload type (video, audio).
         * Any value of the MESH_PAYLOAD_TYPE_* constants.
         */
        int payload_type;
#define MESH_PAYLOAD_TYPE_VIDEO 0 ///< payload: video frames
#define MESH_PAYLOAD_TYPE_AUDIO 1 ///< payload: audio packets

        /**
         * Configuration structures of all payload types
         */
        union {
            MeshConfig_Video video;
            MeshConfig_Audio audio;
        } payload;
    } cfg = {};

    void *grpc_conn;
};

} // namespace mesh

#endif // MESH_CONN_H
