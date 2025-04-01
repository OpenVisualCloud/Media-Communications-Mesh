/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MESH_DP_LEGACY_H
#define __MESH_DP_LEGACY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
     * SMPTE 2110-xx payload type.
     * Typically, should be in the range between 96-127.
     */
    uint8_t payload_type;
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
#define MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE  0 ///< planar YUV 4:2:2, 10bit, "yuv422p10le"
#define MESH_VIDEO_PIXEL_FORMAT_V210              1 ///< packed YUV 4:2:2, 10bit, "v210"
#define MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10 2 ///< packed RFC4175 compliant YUV 4:2:2, 10bit, "yuv422p10rfc4175"

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
 * Connection kind constants (sender, receiver).
 */
#define MESH_CONN_KIND_SENDER   0 ///< Unidirectional connection for sending data
#define MESH_CONN_KIND_RECEIVER 1 ///< Unidirectional connection for receiving data

#ifdef __cplusplus
}
#endif

#endif /* __MESH_DP_LEGACY_H */
