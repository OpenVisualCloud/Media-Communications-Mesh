/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MCM_DP_H
#define __MCM_DP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libmemif.h"

/* Media proxy control commands. */
#define MCM_CREATE_SESSION 1
#define MCM_DESTROY_SESSION 2
#define MCM_QUERY_MEMIF_PATH 3
#define MCM_QUERY_MEMIF_ID 4
#define MCM_QUERY_MEMIF_PARAM 5

// Media proxy magic_word and version
// 4 letters can be casted to numerical value in code:
// header version had been upgraded to 0x20 due to significant changes in the API
#ifndef HEADER_MAGIC_WORD
    #define HEADER_MAGIC_WORD "mcm"
    #define HEADER_VERSION 0x20
#endif

typedef struct _msg_header {
    uint32_t magic_word;
    uint8_t version;
} msg_header;

typedef struct _ctl_cmd {
    uint16_t inst;
    uint16_t data_len;
} ctl_cmd;

typedef struct _mcm_proxy_ctrl_msg {
    msg_header header;
    ctl_cmd command;
    void* data;
} mcm_proxy_ctl_msg;


typedef struct {
    memif_socket_args_t socket_args;
    memif_conn_args_t conn_args;
} memif_conn_param;

typedef struct {
    char socket_path[108];
    uint8_t is_master;
    uint32_t interface_id;
} memif_interface_param;

typedef enum {
    is_tx = 0,
    is_rx,
} transfer_type;

typedef enum {
    PROTO_AUTO = 0,
    PROTO_MEMIF,
    PROTO_UDP,
    PROTO_TCP,
    PROTO_HTTP,
    PROTO_GRPC,
} proto_type;

typedef struct {
    char ip[46];
    char port[6];
} mcm_dp_addr;

typedef struct {
    struct {
        uint16_t seq_num; /* Sequence number */
        uint32_t timestamp; /* Timestamp */
    } metadata; /**< filled by sender side */
    size_t len; /**< size of data filled in "data" */
    void* data;
} mcm_buffer;


/* Mesh client handle type */
typedef void * MeshClient;

/* Mesh connection handle type */
typedef void * MeshConnection;

/* Mesh shared memory buffer type */
typedef mcm_buffer * MeshBuffer;

/* Mesh shared memory buffer information structure */
typedef struct MeshBufferInfo {
    /* Pointer to shared memory area storing data */
    void * const data;

    /* Actual length of data in the buffer */
    const size_t len;

    /* Max length of data in the buffer, i.e. buffer capacity */
    const size_t max_len;
} MeshBufferInfo;

/* Mesh log levels definition */
typedef enum MeshLogLevel {
    MESH_LOG_QUIET = -1,
    MESH_LOG_FATAL = 0,
    MESH_LOG_ERROR,
    MESH_LOG_WARNING,
    MESH_LOG_INFO,
    MESH_LOG_VERBOSE,
    MESH_LOG_DEBUG,
    MESH_LOG_TRACE,
} MeshLogLevel;

/* Error codes */
#define MESH_ERR_BAD_CLIENT_HANDLE          (1000)
#define MESH_ERR_BAD_CONNECTION_HANDLE      (1001)
#define MESH_ERR_BAD_BUFFER_HANDLE          (1002)
#define MESH_ERR_CONNECTION_CLOSED          (1003)
#define MESH_ERR_TIMEOUT                    (1004)
#define MESH_CANNOT_CREATE_MESH_CLIENT      (1005)
#define MESH_CANNOT_CREATE_MESH_CONNECTION  (1006)
#define MESH_CANNOT_CREATE_MEMIF_CONNECTION (1007)

typedef enum {
    MCM_DP_SUCCESS = 0,
    MCM_DP_ERROR_INVALID_PARAM,
    MCM_DP_ERROR_CONNECTION_FAILED,
    MCM_DP_ERROR_TIMEOUT,
    MCM_DP_ERROR_MEMORY_ALLOCATION,
    // Add more error codes as needed
    MCM_DP_ERROR_UNKNOWN = -1
} mcm_dp_error;
/* Maximum number of connections maintained with a Mesh Client*/
#define MAX_NUMBER_OF_CONNECTIONS 2048

/* Mesh client configuration structure */
typedef struct MeshClientConfig {
    uint8_t mesh_version_major;
    uint8_t mesh_version_minor;
    uint8_t mesh_version_hotfix;

   /* Default timeout interval for any API call */
    int timeout_ms;

    /* Max number of streams */
    int max_streams_num;

    /* Log level */
    MeshLogLevel log_level;

    /* Log function*/
    void *mesh_log_fun;    
    
} MeshClientConfig;

typedef enum {
    /* YUV 4:2:0 */
    PIX_FMT_NV12,
    /* YUV 4:2:2 */
    PIX_FMT_YUV422P,
    /* YUV 4:2:2 10bit planar le */
    PIX_FMT_YUV422P_10BIT_LE,
    /* YUV 4:4:4 10bit planar le */
    PIX_FMT_YUV444P_10BIT_LE,
    /* RGB 8bit packed RGB,RGB,...*/
    PIX_FMT_RGB8,
} video_pixel_format;

typedef enum {
    AUDIO_FMT_PCM8 = 0, /**< 8 bits per channel */
    AUDIO_FMT_PCM16, /**< 16 bits per channel */
    AUDIO_FMT_PCM24, /**< 24 bits per channel */
    AUDIO_FMT_AM824, /**< 32 bits per channel */
    AUDIO_FMT_MAX, /**< max value of this enum */
} mcm_audio_format;

typedef enum {
    AUDIO_SAMPLING_48K = 0, /**< sampling rate of 48kHz */
    AUDIO_SAMPLING_96K, /**< sampling rate of 96kHz */
    AUDIO_SAMPLING_44K, /**< sampling rate of 44.1kHz */
    AUDIO_SAMPLING_MAX, /**< max value of this enum */
} mcm_audio_sampling;

typedef enum {
    AUDIO_PTIME_1MS = 0, /**< packet time of 1ms */
    AUDIO_PTIME_125US, /**< packet time of 125us */
    AUDIO_PTIME_250US, /**< packet time of 250us */
    AUDIO_PTIME_333US, /**< packet time of 333us */
    AUDIO_PTIME_4MS, /**< packet time of 4ms */
    AUDIO_PTIME_80US, /**< packet time of 80us */
    AUDIO_PTIME_1_09MS, /**< packet time of 1.09ms, only for 44.1kHz sample */
    AUDIO_PTIME_0_14MS, /**< packet time of 0.14ms, only for 44.1kHz sample */
    AUDIO_PTIME_0_09MS, /**< packet time of 0.09ms, only for 44.1kHz sample */
    AUDIO_PTIME_MAX, /**< max value of this enum */
} mcm_audio_ptime;

typedef enum {
    PAYLOAD_TYPE_NONE = 0,
    PAYLOAD_TYPE_ST20_VIDEO,
    PAYLOAD_TYPE_ST22_VIDEO,
    PAYLOAD_TYPE_ST30_AUDIO,
    PAYLOAD_TYPE_ST40_ANCILLARY,
    PAYLOAD_TYPE_RTSP_VIDEO,
} mcm_payload_type;

typedef enum {
    AUDIO_TYPE_FRAME_LEVEL = 0, /**< app interface lib based on frame level */
    AUDIO_TYPE_RTP_LEVEL, /**< app interface lib based on RTP level */
    AUDIO_TYPE_MAX, /**< max value of this enum */
} mcm_audio_type;

/**
 * Session type of st2110-40(ancillary) streaming
 */
typedef enum {
    ANC_TYPE_FRAME_LEVEL = 0, /**< app interface lib based on frame level */
    ANC_TYPE_RTP_LEVEL, /**< app interface lib based on RTP level */
    ANC_TYPE_MAX, /**< max value of this enum */
} mcm_anc_type;

typedef enum {
    ANC_FORMAT_CLOSED_CAPTION,
    ANC_FORMAT_MAX,
} mcm_anc_format;

typedef enum {
    PAYLOAD_CODEC_NONE = 0,
    PAYLOAD_CODEC_JPEGXS,
    PAYLOAD_CODEC_H264,
} mcm_payload_codec;

/* video format */
typedef struct {
    uint32_t width;
    uint32_t height;
    double fps;
    video_pixel_format pix_fmt;
} mcm_video_args;

/* audio format */
typedef struct {
    /* type */
    mcm_audio_type type;
    /* Audio format */
    mcm_audio_format format;
    /* Number of channels */
    uint16_t channel;
    /* Sample rate */
    mcm_audio_sampling sampling;
    /* packet time */
    mcm_audio_ptime ptime;
} mcm_audio_args;

/* ancillary format */
typedef struct {
    /* type */
    mcm_anc_type type;
    /* Ancillary format */
    mcm_anc_format format;
    double fps;
} mcm_anc_args;

#define MESH_VERSION_MAJOR 24
#define MESH_VERSION_MINOR  9
#define MESH_VERSION_HOTFIX 1


/* Mesh connection */
/* also used as a data structure while connecting to Media Proxy*/
typedef struct MeshConnectionConfig {
    /* data */
    /* connect information */
    transfer_type type;
    proto_type proto;

    /* Media Proxy address */
    mcm_dp_addr* proxy_addr;    

    mcm_dp_addr local_addr;
    mcm_dp_addr remote_addr;    

    /*used for memif sharing directly between two services in one node*/
    memif_interface_param memif_interface;

    mcm_payload_type payload_type;
    mcm_payload_codec payload_codec;
    union {
        mcm_video_args video_args;
        mcm_audio_args audio_args;
        mcm_anc_args anc_args;
    } payload_args;

    int proxy_sockfd;
    uint32_t session_id;
    void* priv;

    /* video resolution */
    uint32_t width;
    uint32_t height;
    double fps;
    video_pixel_format pix_fmt;
    size_t frame_size;

    /* audio */
    mcm_audio_sampling sampling;
    int st30_frame_size;
    int pkt_len;

    uint8_t payload_type_nr;
    uint64_t payload_mtl_flags_mask;
    uint8_t payload_mtl_pacing;    
} MeshConnectionConfig;

typedef struct {
    transfer_type type;
    proto_type protocol;

    mcm_dp_addr local_addr;
    mcm_dp_addr remote_addr;

    /*used for memif sharing directly between two services in one node*/
    memif_interface_param memif_interface;

    mcm_payload_type payload_type;
    mcm_payload_codec payload_codec;
    union {
        mcm_video_args video_args;
        mcm_audio_args audio_args;
        mcm_anc_args anc_args;
    } payload_args;

    /* video format */
    uint32_t width;
    uint32_t height;
    double fps;
    video_pixel_format pix_fmt;

    uint8_t payload_type_nr;
    uint64_t payload_mtl_flags_mask;
    uint8_t payload_mtl_pacing;
} mcm_conn_param;

/* Create a new mesh client */
int mesh_create_client(MeshClient *mc, MeshClientConfig *cfg);

/* Delete mesh client */
int mesh_delete_client(MeshClient *mc);

/**
 * \brief Create session for MCM data plane connection.
 * @param param Parameters for the connect session.
 * \return The context handler of created connect session.
 */
/* Create a new mesh connection */
int mesh_create_connection(MeshClient mc, MeshConnection conn, mcm_conn_param* param);

/**
 * \brief Destroy MCM DP connection.
 * @param pctx The context handler of connection.
 */
/* Delete mesh connection */
int mesh_delete_connection(MeshClient mc, MeshConnection conn);

/* Get buffer from mesh connection */
int mesh_get_buffer(MeshClient mc, MeshConnection conn, MeshBuffer buf, int timeout_ms);

/**
 * Put buffer to buffer queue.
 *
 * For RX side, this function used to return buffer back to the queue.
 * For TX side, this function used to send buffer to RX side.
 *
 * \brief Put buffer to buffer queue.
 * @param conn The context handler of created connect session.
 * @param timeout - timeout in milliseconds
 * Passive event polling -
 * timeout = 0 - dont wait for event, check event queue if there is an event
 * and return. timeout = -1 - wait until event 
 * * \return Error code if failed, return "0" if success.
 */

/* Put buffer to mesh connection */
int mesh_put_buffer(MeshClient mc, MeshConnection conn, MeshBuffer buf, int timeout_ms);

/* Set length of data in the buffer */
int mesh_set_buffer_len(MeshClient mc, MeshConnection conn, MeshBuffer buf, size_t new_len);

/* Log a message */
void mesh_log(MeshClient mc, int log_level, ...);


#ifdef __cplusplus
}
#endif

#endif /* __MCM_DP_H */
