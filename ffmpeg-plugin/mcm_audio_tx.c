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
#include "libavdevice/mcm_common.h"
#include <mcm_dp.h>

typedef struct McmAudioMuxerContext {
    const AVClass *class; /**< Class for private options. */

    /* arguments */
    char *ip_addr;
    char *port;
    char *protocol_type;
    char *payload_type;
    char *socket_name;
    int interface_id;

    mcm_conn_context *tx_handle;
} McmAudioMuxerContext;

static int mcm_audio_write_header(AVFormatContext* avctx)
{
    McmAudioMuxerContext *s = avctx->priv_data;
    mcm_conn_param param = { 0 };
    int err;

    err = mcm_parse_conn_param(&param, is_tx, s->ip_addr, s->port, s->protocol_type,
                               s->payload_type, s->socket_name, s->interface_id);
    if (err)
        return err;

    /* payload type */
    if (param.payload_type != PAYLOAD_TYPE_ST30_AUDIO) {
        av_log(avctx, AV_LOG_ERROR, "Unknown payload type\n");
        return AVERROR(EINVAL);
    }

    /* audio format */
    /* TODO: replace with parameters received from context */
    param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
    param.payload_args.audio_args.channel = 2;
    param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
    param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
    param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;

    s->tx_handle = mcm_create_connection(&param);
    if (!s->tx_handle) {
        av_log(avctx, AV_LOG_ERROR, "Create connection failed\n");
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
            av_log(avctx, AV_LOG_ERROR, "Dequeue buffer error %d\n", err);
            return AVERROR(EIO);
        }

        len = FFMIN(buf->len, size);
        memcpy(buf->data, data, len);
        data += len;
        size -= len;

        buf->len = len;

        if ((err = mcm_enqueue_buffer(s->tx_handle, buf)) != 0) {
            av_log(avctx, AV_LOG_ERROR, "Enqueue buffer error %d\n", err);
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
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = ENC },
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st30"}, .flags = ENC },
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
