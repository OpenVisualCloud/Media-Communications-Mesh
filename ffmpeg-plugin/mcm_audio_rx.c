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
#include "libavformat/internal.h"
#include "libavdevice/mcm_common.h"
#include <mcm_dp.h>

typedef struct McmAudioDemuxerContext {
    const AVClass *class; /**< Class for private options. */

    /* arguments */
    char *ip_addr;
    char *port;
    char *protocol_type;
    char *payload_type;
    char *socket_name;
    int interface_id;

    mcm_conn_context *rx_handle;
    bool first_frame;
} McmAudioDemuxerContext;

static int mcm_audio_read_header(AVFormatContext* avctx)
{
    McmAudioDemuxerContext *s = avctx->priv_data;
    mcm_conn_param param = { 0 };
    AVStream *st;
    int err;

    err = mcm_parse_conn_param(&param, is_rx, s->ip_addr, s->port, s->protocol_type,
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

    s->rx_handle = mcm_create_connection(&param);
    if (!s->rx_handle) {
        av_log(avctx, AV_LOG_ERROR, "Create connection failed\n");
        return AVERROR(EIO);
    }

    st = avformat_new_stream(avctx, NULL);
    if (!st)
        return AVERROR(ENOMEM);

    avpriv_set_pts_info(st, 64, 1, 1000000);

    st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    st->codecpar->codec_id   = AV_NE(AV_CODEC_ID_PCM_S16BE, AV_CODEC_ID_PCM_S16LE);
    st->codecpar->sample_rate = 22050;
    st->codecpar->ch_layout.nb_channels = 2;

    s->first_frame = true;

    /* TODO: replace with parameters received from context */
    av_log(avctx, AV_LOG_INFO,
           "codec:%s sampling:%u ch:%u ptime:1ms\n",
           "pcm16", 20050, 2);
    return 0;
}

static int mcm_audio_read_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmAudioDemuxerContext *s = avctx->priv_data;
    mcm_buffer *buf = NULL;
    int timeout = s->first_frame ? -1 : 1000;
    int err = 0;
    int ret;
    size_t len;

    s->first_frame = false;

    buf = mcm_dequeue_buffer(s->rx_handle, timeout, &err);
    if (buf == NULL) {
        if (!err)
            return AVERROR_EOF;

        av_log(avctx, AV_LOG_ERROR, "Dequeue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    len = buf->len;
    if ((ret = av_new_packet(pkt, len)) < 0)
        return ret;

    memcpy(pkt->data, buf->data, len);

    pkt->pts = pkt->dts = AV_NOPTS_VALUE;

    if ((err = mcm_enqueue_buffer(s->rx_handle, buf)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "Enqueue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    return len;
}

static int mcm_audio_read_close(AVFormatContext* avctx)
{
    McmAudioDemuxerContext* s = avctx->priv_data;

    mcm_destroy_connection(s->rx_handle);
    return 0;
}

#define OFFSET(x) offsetof(McmAudioDemuxerContext, x)
#define DEC (AV_OPT_FLAG_DECODING_PARAM)
static const AVOption mcm_audio_rx_options[] = {
    { "ip_addr", "set remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.1"}, .flags = DEC },
    { "port", "set local port", OFFSET(port), AV_OPT_TYPE_STRING, {.str = "9001"}, .flags = DEC },
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = DEC },
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st20"}, .flags = DEC },
    { "socket_name", "set memif socket name", OFFSET(socket_name), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = DEC },
    { "interface_id", "set interface ID", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, DEC },
    { NULL },
};

static const AVClass mcm_audio_demuxer_class = {
    .class_name = "mcm audio demuxer",
    .item_name = av_default_item_name,
    .option = mcm_audio_rx_options,
    .version = LIBAVUTIL_VERSION_INT,
    .category = AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT,
};

AVInputFormat ff_mcm_audio_demuxer = {
        .name = "mcm_audio",
        .long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio"),
        .priv_data_size = sizeof(McmAudioDemuxerContext),
        .read_header = mcm_audio_read_header,
        .read_packet = mcm_audio_read_packet,
        .read_close = mcm_audio_read_close,
        .flags = AVFMT_NOFILE,
        .priv_class = &mcm_audio_demuxer_class,
};
