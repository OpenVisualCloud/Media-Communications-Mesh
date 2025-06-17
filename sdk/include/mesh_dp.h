/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MESH_DP_H
#define __MESH_DP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Mesh SDK API version
 */
#define MESH_VERSION "25.03"
#define MESH_VERSION_MAJOR  25
#define MESH_VERSION_MINOR  3
#define MESH_VERSION_HOTFIX 0

/**
 * Mesh client structure
 */
typedef struct{} MeshClient;

/**
 * Mesh connection structure
 */
typedef struct MeshConnection {
    /**
     * Parent mesh client;
     */
    MeshClient * const client;
    /**
     * Payload size, or frame size, configured for the connection.
     * This value is the maximum length of payload the buffer may contain.
     * It is calculated once before the connection is created and cannot be
     * altered thereafter. The calculation is based on the payload type and
     * payload parameters.
     * For video payload, this value is the video frame size.
     * For audio payload, this value is the audio packet size.
     */
    const size_t payload_size;
    /**
     * Metadata size, configured for the connection.
     * This value is the maximum length of metadata the buffer may contain.
     */
    const size_t metadata_size;

} MeshConnection;

/**
 * Mesh shared memory buffer type
 */
typedef struct{
    /**
     * Parent mesh connection
     */
    MeshConnection * const conn;
    /**
     * Pointer to shared memory area storing data
     */
    void * const payload_ptr;
    /**
     * Actual length of data in the buffer
     */
    const size_t payload_len;
    /**
     * Pointer to shared memory area storing metadata
     */
    void * const metadata_ptr;
    /**
     * Actual lenght of metadata in the buffer
     */
    const size_t metadata_len;

} MeshBuffer;

/**
 * Timeout configuration constants
 */
#define MESH_TIMEOUT_DEFAULT  -2 ///< Use default timeout defined for mesh client
#define MESH_TIMEOUT_INFINITE -1 ///< No timeout, block until success or error
#define MESH_TIMEOUT_ZERO      0 ///< Polling mode, return immediately

/**
 * Error codes
 */
#define MESH_ERR_BAD_CLIENT_PTR       1000 ///< Bad client pointer
#define MESH_ERR_BAD_CONN_PTR         1001 ///< Bad connection pointer
#define MESH_ERR_BAD_CONFIG_PTR       1002 ///< Bad configuration pointer
#define MESH_ERR_BAD_BUF_PTR          1003 ///< Bad buffer pointer
#define MESH_ERR_BAD_BUF_LEN          1004 ///< Bad buffer length
#define MESH_ERR_CLIENT_CONFIG_INVAL  1005 ///< Invalid client config
#define MESH_ERR_MAX_CONN             1006 ///< Reached max connections number
#define MESH_ERR_FOUND_ALLOCATED      1007 ///< Found allocated resources
#define MESH_ERR_CONN_FAILED          1008 ///< Connection creation failed
#define MESH_ERR_CONN_CONFIG_INVAL    1009 ///< Invalid connection config
#define MESH_ERR_CONN_CONFIG_INCOMPAT 1010 ///< Incompatible connection config
#define MESH_ERR_CONN_CLOSED          1011 ///< Connection is closed
#define MESH_ERR_TIMEOUT              1012 ///< Timeout occurred
#define MESH_ERR_NOT_IMPLEMENTED      1013 ///< Feature not implemented yet

/**
 * @brief Create a new mesh client.
 *
 * Creates a new mesh client from the given json configuration structure.
 *
 * @param [out] mc Address of a pointer to a mesh client structure.
 * @param [in] cfg Pointer to a json configuration string.
 *
 * @return 0 on success; an error code otherwise.
 */
int mesh_create_client(MeshClient **mc, const char *cfg);

/**
 * @brief Delete mesh client.
 * 
 * Deletes the mesh client and its resources.
 * 
 * @param [in,out] mc Address of a pointer to a mesh client structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_delete_client(MeshClient **mc);

/**
 * @brief Create a new mesh transmitter connection.
 * 
 * Creates a new mesh transmitter connection from the given json configuration
 * structure.
 * 
 * @param [in] mc Pointer to a parent mesh client.
 * @param [out] conn Address of a pointer to the connection structure.
 * @param [in] cfg Pointer to a json configuration string.
 *
 * @return 0 on success; an error code otherwise.
 */
int mesh_create_tx_connection(MeshClient *mc, MeshConnection **conn, const char *cfg);

/**
 * @brief Create a new mesh receiver connection.
 * 
 * Creates a new mesh receiver connection from the given json configuration
 * structure.
 * 
 * @param [in] mc Pointer to a parent mesh client.
 * @param [out] conn Address of a pointer to the connection structure.
 * @param [in] cfg Pointer to a json configuration string.
 *
 * @return 0 on success; an error code otherwise.
 */
int mesh_create_rx_connection(MeshClient *mc, MeshConnection **conn, const char *cfg);

/**
 * @brief Shutdown a mesh connection.
 * 
 * Closes the active mesh connection.
 * 
 * @param [in] conn Pointer to a connection structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_shutdown_connection(MeshConnection *conn);

/**
 * @brief Delete mesh connection.
 * 
 * Deletes the connection and its resources.
 * 
 * @param [in,out] conn Address of a pointer to the connection structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_delete_connection(MeshConnection **conn);

/**
 * @brief Get buffer from mesh connection.
 * 
 * @param [in] conn Pointer to a connection structure.
 * @param [out] buf Address of a pointer to a mesh buffer structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_get_buffer(MeshConnection *conn, MeshBuffer **buf);

/**
 * @brief Get buffer from mesh connection with timeout.
 * 
 * @param [in] conn Pointer to a connection structure.
 * @param [out] buf Address of a pointer to a mesh buffer structure.
 * @param [in] timeout_ms Timeout interval in milliseconds.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_get_buffer_timeout(MeshConnection *conn, MeshBuffer **buf,
                            int timeout_ms);

/**
 * @brief Put buffer to mesh connection.
 * 
 * @param [in,out] buf Address of a pointer to a mesh buffer structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_put_buffer(MeshBuffer **buf);

/**
 * @brief Put buffer to mesh connection with timeout.
 * 
 * @param [in,out] buf Address of a pointer to a mesh buffer structure.
 * @param [in] timeout_ms Timeout interval in milliseconds.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_put_buffer_timeout(MeshBuffer **buf, int timeout_ms);

/**
 * @brief Set payload length of a mesh buffer.
 *
 * @param [in] buf Pointer to a mesh buffer structure.
 * @param [in] len Payload length in bytes.
 *
 * @return 0 on success; an error code otherwise.
 */
int mesh_buffer_set_payload_len(MeshBuffer *buf, size_t len);

/**
 * @brief Set metadata length of a mesh buffer.
 *
 * @param [in] buf Pointer to a mesh buffer structure.
 * @param [in] len Metadata length in bytes.
 *
 * @return 0 on success; an error code otherwise.
 */
int mesh_buffer_set_metadata_len(MeshBuffer *buf, size_t len);

/**
 * @brief Get text description of an error code.
 * 
 * @param [in] err Error code returned from any Mesh Data Plane API call.
 * 
 * @return NULL-terminated string describing the error code.
 */
const char * mesh_err2str(int err);

#ifdef __cplusplus
}
#endif

#endif /* __MESH_DP_H */
