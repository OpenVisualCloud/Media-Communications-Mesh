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

    int channels;
    int sample_rate;
    char* ptime;

    mcm_conn_context *tx_handle;
} McmAudioMuxerContext;

static int mcm_audio_write_header(AVFormatContext* avctx)
{
    AVCodecParameters* codecpar = avctx->streams[0]->codecpar;
    McmAudioMuxerContext *s = avctx->priv_data;
    mcm_audio_sampling mcm_sample_rate;
    mcm_conn_param param = { 0 };
    mcm_audio_ptime mcm_ptime;
    mcm_audio_format mcm_fmt;
    int err;

    err = mcm_parse_conn_param(avctx, &param, is_tx, s->ip_addr, s->port,
                               s->protocol_type, s->payload_type, s->socket_name,
                               s->interface_id);
    if (err)
        return err;

    /* payload type */
    if (param.payload_type != PAYLOAD_TYPE_ST30_AUDIO) {
        av_log(avctx, AV_LOG_ERROR, "Unknown payload type\n");
        return AVERROR(EINVAL);
    }

    /* check channels argument */
    if (codecpar->ch_layout.nb_channels != s->channels) {
        av_log(avctx, AV_LOG_ERROR, "Source audio stream is of %d channels, not %d\n",
               codecpar->ch_layout.nb_channels, s->channels);
        return AVERROR(EINVAL);
    }

    /* check sample_rate argument */
    if (codecpar->sample_rate != s->sample_rate) {
        av_log(avctx, AV_LOG_ERROR, "Source audio stream sample rate is %d, not %d\n",
               codecpar->sample_rate, s->sample_rate);
        return AVERROR(EINVAL);
    }

    err = mcm_parse_audio_sample_rate(avctx, &mcm_sample_rate, s->sample_rate);
    if (err)
        return err;

    err = mcm_parse_audio_packet_time(avctx, &mcm_ptime, s->ptime);
    if (err)
        return err;

    switch (codecpar->codec_id) {
    case AV_CODEC_ID_PCM_S24BE:
        mcm_fmt = AUDIO_FMT_PCM24;
        break;
    case AV_CODEC_ID_PCM_S16BE:
        mcm_fmt = AUDIO_FMT_PCM16;
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Audio PCM format not supported\n");
        return AVERROR(EINVAL);
    }

    /* audio format */
    param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
    param.payload_args.audio_args.channel = s->channels;
    param.payload_args.audio_args.sampling = mcm_sample_rate;
    param.payload_args.audio_args.ptime = mcm_ptime;
    param.payload_args.audio_args.format = mcm_fmt;

    s->tx_handle = mcm_create_connection(&param);
    if (!s->tx_handle) {
        av_log(avctx, AV_LOG_ERROR, "Create connection failed\n");
        return AVERROR(EIO);
    }

    av_log(avctx, AV_LOG_INFO, "codec:%s sampling:%d ch:%d ptime:%s\n",
           avcodec_get_name(codecpar->codec_id), s->sample_rate, s->channels,
           s->ptime);
    return 0;
}

static int mcm_audio_write_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmAudioMuxerContext *s = avctx->priv_data;
    const uint8_t *data = pkt->data;
    mcm_buffer *buf = NULL;
    int size = pkt->size;
    int len, err = 0;

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
    { "interface_id", "set interface id", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, ENC },
    { "channels", "number of audio channels", OFFSET(channels), AV_OPT_TYPE_INT, {.i64 = 2}, 1, INT_MAX, ENC },
    { "sample_rate", "audio sample rate", OFFSET(sample_rate), AV_OPT_TYPE_INT, {.i64 = 48000}, 1, INT_MAX, ENC },
    { "ptime", "audio packet time", OFFSET(ptime), AV_OPT_TYPE_STRING, {.str = "1ms"}, .flags = ENC },
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
    .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio pcm24"),
    .priv_data_size = sizeof(McmAudioMuxerContext),
    .write_header = mcm_audio_write_header,
    .write_packet = mcm_audio_write_packet,
    .write_trailer = mcm_audio_write_trailer,
    .p.audio_codec = AV_CODEC_ID_PCM_S24BE,
    .p.video_codec = AV_CODEC_ID_NONE,
    .p.flags = AVFMT_NOFILE,
    .p.priv_class = &mcm_audio_muxer_class,
};

const FFOutputFormat ff_mcm_audio_pcm16_muxer = {
    .p.name = "mcm_audio_pcm16",
    .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio pcm16"),
    .priv_data_size = sizeof(McmAudioMuxerContext),
    .write_header = mcm_audio_write_header,
    .write_packet = mcm_audio_write_packet,
    .write_trailer = mcm_audio_write_trailer,
    .p.audio_codec = AV_CODEC_ID_PCM_S16BE,
    .p.video_codec = AV_CODEC_ID_NONE,
    .p.flags = AVFMT_NOFILE,
    .p.priv_class = &mcm_audio_muxer_class,
};
