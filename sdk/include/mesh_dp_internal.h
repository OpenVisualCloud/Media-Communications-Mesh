/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MESH_DP_INTERNAL_H
#define __MESH_DP_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mesh_dp.h"
#include "mcm_dp.h"
#include <sys/queue.h>

/**
 * Mesh connection context structure
 */
typedef struct MeshConnectionContext {
    /**
     * NOTE: The __public structure is directly mapped in the memory to the
     * MeshConnection structure, which is publicly accessible to the user.
     * Therefore, the __public structure _MUST_ be placed first here.
     */
    MeshConnection __public;

    /**
     * NOTE: All declarations below this point are hidden from the user.
     */

    /**
     * Connections list entry registered in the Mesh client.
     */
    LIST_ENTRY(MeshConnectionContext) conns;

    /**
     * MCM connection handle.
     */
    mcm_conn_context *handle;

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
    } cfg;

} MeshConnectionContext;

/**
 * Max number of connections handled by mesh client by default
 */
#define MESH_CLIENT_DEFAULT_MAX_CONN 1024

/**
 * Default timeout applied to all mesh client operations
 */
#define MESH_CLIENT_DEFAULT_TIMEOUT_MS (MESH_TIMEOUT_INFINITE)

/**
 * Constants for marking uninitialized resources
 */
#define MESH_CONN_TYPE_UNINITIALIZED    -1 ///< Connection type is uninitialized
#define MESH_PAYLOAD_TYPE_UNINITIALIZED -1 ///< Payload type is uninitialized

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct mesh_internal_ops_t {
    mcm_conn_context * (*create_conn)(mcm_conn_param *param);
    void (*destroy_conn)(mcm_conn_context *pctx);
    mcm_buffer * (*dequeue_buf)(mcm_conn_context *pctx, int timeout, int *error_code);
    int (*enqueue_buf)(mcm_conn_context *pctx, mcm_buffer *buf);
};

extern struct mesh_internal_ops_t mesh_internal_ops;

/**
 * Parse payload configuration
 */
int mesh_parse_payload_config(MeshConnectionContext *conn, mcm_conn_param *param);
/**
 * Parse connection configuration and check for for wrong or incompatible
 * parameter values.
 */
int mesh_parse_conn_config(MeshConnectionContext *ctx, mcm_conn_param *param);

#ifdef __cplusplus
}
#endif

#endif /* __MESH_DP_INTERNAL_H */
