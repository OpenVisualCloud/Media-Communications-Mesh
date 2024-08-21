/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/internal.h"
#include "libavformat/avformat.h"
#include "libavformat/mux.h"
#include <mcm_dp.h>
#include <bsd/string.h>

typedef struct McmAudioMuxerContext {
    const AVClass *class; /**< Class for private options. */

    /* arguments */
    char *ip_addr;
    char *port;
    char *payload_type;
    char *protocol_type;
    char *socket_name;
    int interface_id;

    mcm_conn_context *tx_handle;
} McmAudioMuxerContext;

static int mcm_audio_write_header(AVFormatContext* avctx)
{
    McmAudioMuxerContext *s = avctx->priv_data;
    mcm_conn_param param = { 0 };

    strlcpy(param.remote_addr.ip, s->ip_addr, sizeof(param.remote_addr.ip));
    strlcpy(param.remote_addr.port, s->port, sizeof(param.remote_addr.port));

    /* protocol type */
    if (strcmp(s->protocol_type, "memif") == 0) {
        param.protocol = PROTO_MEMIF;
        param.memif_interface.is_master = 1;
        snprintf(param.memif_interface.socket_path, sizeof(param.memif_interface.socket_path),
            "/run/mcm/mcm_memif_%s.sock", s->socket_name ? s->socket_name : "0");
        param.memif_interface.interface_id = s->interface_id;
    } else if (strcmp(s->protocol_type, "udp") == 0) {
        param.protocol = PROTO_UDP;
    } else if (strcmp(s->protocol_type, "tcp") == 0) {
        param.protocol = PROTO_TCP;
    } else if (strcmp(s->protocol_type, "http") == 0) {
        param.protocol = PROTO_HTTP;
    } else if (strcmp(s->protocol_type, "grpc") == 0) {
        param.protocol = PROTO_GRPC;
    } else {
        param.protocol = PROTO_AUTO;
    }

    /* payload type */
    if (strcmp(s->payload_type, "st30") == 0) {
        param.payload_type = PAYLOAD_TYPE_ST30_AUDIO;
    } else {
        av_log(avctx, AV_LOG_ERROR, "unknown payload type\n");
        return AVERROR(EINVAL);
    }

    /* audio format */
    /* TODO: replace with parameters received from context */
    param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
    param.payload_args.audio_args.channel = 2;
    param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
    param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
    param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;

    param.type = is_tx;

    s->tx_handle = mcm_create_connection(&param);
    if (!s->tx_handle) {
        av_log(avctx, AV_LOG_ERROR, "create connection failed\n");
        return AVERROR(EIO);
    }

    /* TODO: replace with parameters received from context */
    av_log(avctx, AV_LOG_INFO,
           "codec:%s sampling:%u ch:%u ptime:1ms\n",
           "pcm16", 20050, 2);
    return 0;
}

static int mcm_audio_write_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmAudioMuxerContext *s = avctx->priv_data;
    mcm_buffer *buf = NULL;
    int err = 0;
    int size = pkt->size;
    const uint8_t *data = pkt->data;
    int len;

    while (size > 0) {
        buf = mcm_dequeue_buffer(s->tx_handle, -1, &err);
        if (buf == NULL) {
            av_log(avctx, AV_LOG_ERROR, "dequeue buffer error %d\n", err);
            return AVERROR(EIO);
        }

        len = FFMIN(buf->len, size);
        memcpy(buf->data, data, len);
        data += len;
        size -= len;

        buf->len = len;

        if ((err = mcm_enqueue_buffer(s->tx_handle, buf)) != 0) {
            av_log(avctx, AV_LOG_ERROR, "enqueue buffer error %d\n", err);
            return AVERROR(EIO);
        }

    }

    return 0;
}

static int mcm_audio_write_trailer(AVFormatContext* avctx)
{
    McmAudioMuxerContext *s = avctx->priv_data;

    mcm_destroy_connection(s->tx_handle);
    return 0;
}

#define OFFSET(x) offsetof(McmAudioMuxerContext, x)
#define ENC AV_OPT_FLAG_ENCODING_PARAM
static const AVOption mcm_audio_tx_options[] = {
    { "ip_addr", "set remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.2"}, .flags = ENC },
    { "port", "set remote port", OFFSET(port), AV_OPT_TYPE_STRING, {.str = "9001"}, .flags = ENC },
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st30"}, .flags = ENC },
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = ENC },
    { "socket_name", "set memif socket name", OFFSET(socket_name), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = ENC },
    { "interface_id", "set interface ID", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, ENC },
    { NULL },
};

static const AVClass mcm_audio_muxer_class = {
    .class_name = "mcm audio muxer",
    .item_name = av_default_item_name,
    .option = mcm_audio_tx_options,
    .version = LIBAVUTIL_VERSION_INT,
    .category = AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT,
};

const FFOutputFormat ff_mcm_audio_muxer = {
    .p.name = "mcm_audio",
    .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio"),
    .priv_data_size = sizeof(McmAudioMuxerContext),
    .write_header = mcm_audio_write_header,
    .write_packet = mcm_audio_write_packet,
    .write_trailer = mcm_audio_write_trailer,
    .p.audio_codec = AV_NE(AV_CODEC_ID_PCM_S16BE, AV_CODEC_ID_PCM_S16LE),
    .p.video_codec = AV_CODEC_ID_NONE,
    .p.flags = AVFMT_NOFILE,
    .p.priv_class = &mcm_audio_muxer_class,
};
