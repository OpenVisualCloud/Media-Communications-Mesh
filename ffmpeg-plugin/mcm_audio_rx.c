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
#include <mcm_dp.h>
#include <bsd/string.h>

typedef struct McmAudioDemuxerContext {
    const AVClass *class; /**< Class for private options. */

    /* arguments */
    char *ip_addr;
    char *port;
    char *payload_type;
    char *protocol_type;
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

    strlcpy(param.remote_addr.ip, s->ip_addr, sizeof(param.remote_addr.ip));
    strlcpy(param.local_addr.port, s->port, sizeof(param.local_addr.port));

    /* protocol type */
    if (strcmp(s->protocol_type, "memif") == 0) {
        param.protocol = PROTO_MEMIF;
        param.memif_interface.is_master = 0;
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

    param.type = is_rx;

    s->rx_handle = mcm_create_connection(&param);
    if (!s->rx_handle) {
        av_log(avctx, AV_LOG_ERROR, "create connection failed\n");
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

        av_log(avctx, AV_LOG_ERROR, "dequeue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    len = buf->len;
    if ((ret = av_new_packet(pkt, len)) < 0)
        return ret;

    memcpy(pkt->data, buf->data, len);

    pkt->pts = pkt->dts = AV_NOPTS_VALUE;

    if ((err = mcm_enqueue_buffer(s->rx_handle, buf)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "enqueue buffer error %d\n", err);
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
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st20"}, .flags = DEC },
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = DEC },
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
