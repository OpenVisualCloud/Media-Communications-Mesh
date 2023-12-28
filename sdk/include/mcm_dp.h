/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
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

typedef enum {
    MCM_DP_SUCCESS = 0,
    MCM_DP_ERROR_INVALID_PARAM,
    MCM_DP_ERROR_CONNECTION_FAILED,
    MCM_DP_ERROR_TIMEOUT,
    MCM_DP_ERROR_MEMORY_ALLOCATION,
    // Add more error codes as needed
    MCM_DP_ERROR_UNKNOWN = -1
} mcm_dp_error;

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

typedef enum {
    /* YUV 4:2:0 */
    PIX_FMT_NV12,
    /* YUV 4:2:2 */
    PIX_FMT_YUV422P,
    /* YUV 4:4:4 */
    PIX_FMT_YUV444M,
    /* RGB */
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
} mcm_conn_param;

typedef struct _mcm_conn_context mcm_conn_context;
typedef struct _mcm_conn_context {
    /* data */
    /* connect information */
    transfer_type type;
    int proxy_sockfd;
    uint32_t session_id;
    proto_type proto;
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

    /* function */
    mcm_buffer* (*dequeue_buffer)(mcm_conn_context* self, int timeout, int* error_code);
    int (*enqueue_buffer)(mcm_conn_context* self, mcm_buffer* buf);
} mcm_conn_context;

/**
 * \brief Create session for MCM data plane connection.
 * @param param Parameters for the connect session.
 * \return The context handler of created connect session.
 */
mcm_conn_context* mcm_create_connection(mcm_conn_param* param);

/**
 * \brief Destroy MCM DP connection.
 * @param pctx The context handler of connection.
 */
void mcm_destroy_connection(mcm_conn_context* pctx);

/**
 * Get buffer from buffer queue.
 *
 * For TX side, this function used to alloc buffer from buffer queue.
 * For RX side, this function used to read buffer from TX side.
 *
 * \brief Get buffer from buffer queue.
 * @param pctx The context handler of created connect session.
 * @param timeout - timeout in milliseconds
 * Passive event polling -
 * timeout = 0 - dont wait for event, check event queue if there is an event
 * and return. timeout = -1 - wait until event
 * @param error_code Error code if failed, can be set to NULL if doesn't care.
 * \return Pointer to the mcm_buffer, return NULL if failed.
 */
mcm_buffer* mcm_dequeue_buffer(mcm_conn_context* pctx, int timeout, int* error_code);

/**
 * Put buffer to buffer queue.
 *
 * For RX side, this function used to return buffer back to the queue.
 * For TX side, this function used to send buffer to RX side.
 *
 * \brief Put buffer to buffer queue.
 * @param pctx The context handler of created connect session.
 * @param buf Pinter to the mcm_buffer.
 * \return Error code if failed, return "0" if success.
 */
int mcm_enqueue_buffer(mcm_conn_context* pctx, mcm_buffer* buf);

#ifdef __cplusplus
}
#endif

#endif /* __MCM_DP_H */
