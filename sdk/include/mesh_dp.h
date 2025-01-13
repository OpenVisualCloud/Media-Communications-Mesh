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
#define MESH_VERSION "24.09"
#define MESH_VERSION_MAJOR  24
#define MESH_VERSION_MINOR  9
#define MESH_VERSION_HOTFIX 0

/**
 * Mesh client structure
 */
typedef struct{} MeshClient;

/**
 * Mesh client configuration structure
 */
typedef struct MeshClientConfig {
    /**
     * Default timeout interval for any API call
     */
    int timeout_ms;

    /**
     * Max number of connections
     */
    int max_conn_num;

    /**
     * TEMPORARY
     * TCP connection is used by default. This flag is to enable gRPC instead.
     */
    bool enable_grpc;

} MeshClientConfig;

/**
 * Mesh connection structure
 */
typedef struct MeshConnection {
    /**
     * Parent mesh client;
     */
    MeshClient * const client;
    /**
     * Buffer size, or frame size, configured for the connection.
     * This value is the maximum length of data the buffer may contain.
     * It is calculated once before the connection is created and cannot be
     * altered thereafter. The calculation is based on the payload type and
     * payload parameters.
     * For video payload, this value is the video frame size.
     * For audio payload, this value is the audio packet size.
     */
    const size_t buf_size;

} MeshConnection;

/**
 * Define configuration string field sizes
 */
#define MESH_SOCKET_PATH_SIZE   108
#define MESH_IP_ADDRESS_SIZE    253 /* max of [IPv4, IPv6, FQDN] */

/**
 * Mesh configuration for Single node direct connection via memif
 */
typedef struct MeshConfig_Memif {
    /**
     * Memif socket path.
     * Example: /run/mcm/mcm_memif_0.sock
     */
    char socket_path[MESH_SOCKET_PATH_SIZE];
    /**
     * Memif interface id.
     * Default: 0
     */
    int interface_id;

} MeshConfig_Memif;

/**
 * Mesh configuration for SMPTE ST2110-xx connection via Media Proxy
 */
typedef struct MeshConfig_ST2110 {
    /**
     * Remote IP address
     */
    char remote_ip_addr[MESH_IP_ADDRESS_SIZE];
    /**
     * Remote port
     */
    uint16_t remote_port;
    /**
     * Local IP address
     */
    char local_ip_addr[MESH_IP_ADDRESS_SIZE];
    /**
     * Local port
     */
    uint16_t local_port;
    /**
     * SMPTE ST2110-xx transport type.
     * Must be aligned with the payload type.
     * Any value of the MESH_CONN_TRANSPORT_ST2110_* constants.
     */
    int transport;
#define MESH_CONN_TRANSPORT_ST2110_20 0 ///< SMPTE ST2110-20 Uncompressed Video transport
#define MESH_CONN_TRANSPORT_ST2110_22 1 ///< SMPTE ST2110-22 Constant Bit-Rate Compressed Video transport
#define MESH_CONN_TRANSPORT_ST2110_30 2 ///< SMPTE ST2110-30 Audio transport

    /**
     * SMPTE ST2110-20 rfc4175 compliant transport format.
     * Required only for ST2110-20 transport.
     * Any value of the MESH_CONN_ST2110_20_TRANSPORT_FMT_* constants.
     */
    int transport_format;
#define MESH_CONN_ST2110_20_TRANSPORT_FMT_YUV422_10BIT 0 ///< YUV 4:2:2 10-bit, "yuv422p10rfc4175"

} MeshConfig_ST2110;

/**
 * Mesh configuration for RDMA connection via Media Proxy
 */
typedef struct MeshConfig_RDMA {
            /**
             * Remote IP address
             */
            char remote_ip_addr[MESH_IP_ADDRESS_SIZE];
            /**
             * Remote port
             */
            uint16_t remote_port;
            /**
             * Local IP address
             */
            char local_ip_addr[MESH_IP_ADDRESS_SIZE];
            /**
             * Local port
             */
            uint16_t local_port;

} MeshConfig_RDMA;

/**
 * Mesh payload configuration for Video frames
 */
typedef struct MeshConfig_Video {
    /**
     * Video frame width in pixels.
     */
    int width;
    /**
     * Video frame height in pixels.
     */
    int height;
    /**
     * Video frames per second.
     */
    double fps;
    /**
     * Video frame pixel format.
     * Any value of the MESH_VIDEO_PIXEL_FORMAT_* constants.
     */
    int pixel_format;
#define MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE      0 ///< planar YUV 4:2:2, 10bit, "yuv422p10le"
#define MESH_VIDEO_PIXEL_FORMAT_V210                  1 ///< packed YUV 4:2:2, 10bit, "v210"
#define MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10     2 ///< packed RFC4175 compliant YUV 4:2:2, 10bit, "yuv422p10rfc4175"

} MeshConfig_Video;

/**
 * Mesh payload configuration for Audio packets
 */
typedef struct MeshConfig_Audio {
    /**
     * Number of audio channels (1, 2, 4, etc.)
     */
    int channels;
    /**
     * Audio sample rate.
     * Any value of the MESH_AUDIO_SAMPLE_RATE_* constants.
     */
    int sample_rate;
#define MESH_AUDIO_SAMPLE_RATE_48000 0 ///< Audio sample rate 48000 Hz
#define MESH_AUDIO_SAMPLE_RATE_96000 1 ///< Audio sample rate 96000 Hz
#define MESH_AUDIO_SAMPLE_RATE_44100 2 ///< Audio sample rate 44100 Hz

    /**
     * Audio sample format.
     * Any value of the MESH_AUDIO_FORMAT_* constants.
     */
    int format;
#define MESH_AUDIO_FORMAT_PCM_S8    0 ///< PCM 8 bits per channel
#define MESH_AUDIO_FORMAT_PCM_S16BE 1 ///< PCM 16 bits per channel, big endian
#define MESH_AUDIO_FORMAT_PCM_S24BE 2 ///< PCM 24 bits per channel, big endian

    /**
     * Audio packet time.
     * Any value of the MESH_AUDIO_PACKET_TIME_* constants.
     */
    int packet_time;
/** Constants for 48kHz and 96kHz sample rates */
#define MESH_AUDIO_PACKET_TIME_1MS      0 ///< Audio packet time 1ms
#define MESH_AUDIO_PACKET_TIME_125US    1 ///< Audio packet time 125us
#define MESH_AUDIO_PACKET_TIME_250US    2 ///< Audio packet time 250us
#define MESH_AUDIO_PACKET_TIME_333US    3 ///< Audio packet time 333us
#define MESH_AUDIO_PACKET_TIME_4MS      4 ///< Audio packet time 4ms
#define MESH_AUDIO_PACKET_TIME_80US     5 ///< Audio packet time 80us
/** Constants for 44.1kHz sample rate */
#define MESH_AUDIO_PACKET_TIME_1_09MS   6 ///< Audio packet time 1.09ms
#define MESH_AUDIO_PACKET_TIME_0_14MS   7 ///< Audio packet time 0.14ms
#define MESH_AUDIO_PACKET_TIME_0_09MS   8 ///< Audio packet time 0.09ms

} MeshConfig_Audio;

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
    union {
        void *const data; /*deprecated*/
        void *const payload_ptr;
    };
    /**
     * Actual length of data in the buffer
     */
    union {
        const size_t data_len; /*deprecated*/
        const size_t payload_len;
    };
    /**
     * Pointer to shared memory area storing metadata
     */
    void *const metadata_ptr;
    /**
     * Actual lenght of metadata in the buffer
     */
    const size_t metadata_len;

} MeshBuffer;

/**
 * Connection kind constants (sender, receiver).
 */
#define MESH_CONN_KIND_SENDER   0 ///< Unidirectional connection for sending data
#define MESH_CONN_KIND_RECEIVER 1 ///< Unidirectional connection for receiving data

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
#define MESH_ERR_CLIENT_CONFIG_INVAL  1004 ///< Invalid client config
#define MESH_ERR_MAX_CONN             1005 ///< Reached max connections number
#define MESH_ERR_FOUND_ALLOCATED      1006 ///< Found allocated resources
#define MESH_ERR_CONN_FAILED          1007 ///< Connection creation failed
#define MESH_ERR_CONN_CONFIG_INVAL    1008 ///< Invalid connection config
#define MESH_ERR_CONN_CONFIG_INCOMPAT 1009 ///< Incompatible connection config
#define MESH_ERR_CONN_CLOSED          1010 ///< Connection is closed
#define MESH_ERR_TIMEOUT              1011 ///< Timeout occurred
#define MESH_ERR_NOT_IMPLEMENTED      1012 ///< Feature not implemented yet

/**
 * @brief Create a new mesh client.
 * 
 * Creates a new mesh client from the given configuration structure.
 * 
 * @param [out] mc Address of a pointer to a mesh client structure.
 * @param [in] cfg Pointer to a mesh client configuration structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_create_client(MeshClient **mc, MeshClientConfig *cfg);

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
int mesh_create_client_json(MeshClient **mc, const char *cfg);

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
 * @brief Create a new mesh connection.
 * 
 * Creates a new media connection for the given mesh client.
 * 
 * @param [in] mc Pointer to a parent mesh client.
 * @param [out] conn Address of a pointer to the connection structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_create_connection(MeshClient *mc, MeshConnection **conn);

/**
 * @brief Create a new mesh transmitter connection.
 * @param [in] mc Pointer to a parent mesh client.
 * @param [in] cfg Pointer to a json configuration string.
 * @param [out] conn Address of a pointer to the connection structure.
 */
int mesh_create_tx_connection(MeshClient *mc, MeshConnection **conn, const char *cfg);

/**
 * @brief Create a new mesh receiver connection.
 * @param [in] mc Pointer to a parent mesh client.
 * @param [in] cfg Pointer to a json configuration string.
 * @param [out] conn Address of a pointer to the connection structure.
 */
int mesh_create_rx_connection(MeshClient *mc, MeshConnection **conn, const char *cfg);

/**
 * @brief Apply configuration to setup Single node direct connection via memif.
 * 
 * @param [in] conn Pointer to a connection structure.
 * @param [in] cfg Pointer to a configuration structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_apply_connection_config_memif(MeshConnection *conn, MeshConfig_Memif *cfg);

/**
 * @brief Apply configuration to setup SMPTE ST2110-xx connection via Media Proxy.
 * 
 * @param [in] conn Pointer to a connection structure.
 * @param [in] cfg Pointer to a configuration structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_apply_connection_config_st2110(MeshConnection *conn, MeshConfig_ST2110 *cfg);

/**
 * @brief Apply configuration to setup RDMA connection via Media Proxy.
 * 
 * @param [in] conn Pointer to a connection structure.
 * @param [in] cfg Pointer to a configuration structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_apply_connection_config_rdma(MeshConnection *conn, MeshConfig_RDMA *cfg);

/**
 * @brief Apply configuration to setup connection payload for Video frames.
 * 
 * @param [in] conn Pointer to a connection structure.
 * @param [in] cfg Pointer to a configuration structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_apply_connection_config_video(MeshConnection *conn, MeshConfig_Video *cfg);

/**
 * @brief Apply configuration to setup connection payload for Audio packets.
 * 
 * @param [in] conn Pointer to a connection structure.
 * @param [in] cfg Pointer to a configuration structure.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_apply_connection_config_audio(MeshConnection *conn, MeshConfig_Audio *cfg);

/**
 * @brief Establish a mesh connection.
 * 
 * Checks the previously applied configuration for parameter compatibility
 * and establishes a mesh connection of the given kind (sender or receiver).
 * 
 * @param [in] conn Pointer to a connection structure.
 * @param [in] kind Connection kind: Sender or Receiver.
 * 
 * @return 0 on success; an error code otherwise.
 */
int mesh_establish_connection(MeshConnection *conn, int kind);

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
 * @param [in,out] buf Address of a pointer to a mesh buffer structure.
 *
 * @return 0 on success; an error code otherwise.
 */
int mesh_buffer_set_payload_len(MeshBuffer **buf,
                                size_t len);

/**
 * @brief Set metadata length of a mesh buffer.
 *
 * @param [in,out] buf Address of a pointer to a mesh buffer structure.
 *
 * @return 0 on success; an error code otherwise.
 */
int mesh_buffer_set_metadata_len(MeshBuffer **buf,
                                 size_t len);

/**
 * @brief Get text description of an error code.
 * 
 * @param [in] err Error code returned from any Mesh Data Plane API call.
 * 
 * @return NULL-terminated string describing the error code.
 */
const char *mesh_err2str(int err);

#ifdef __cplusplus
}
#endif

#endif /* __MESH_DP_H */
