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
#ifdef MCM_FFMPEG_7_0
#include "libavformat/demux.h"
#endif /* MCM_FFMPEG_7_0 */

typedef struct McmAudioDemuxerContext {
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
    char* pcm_format;

    mcm_conn_context *rx_handle;
    bool first_frame;
} McmAudioDemuxerContext;

static int mcm_audio_read_header(AVFormatContext* avctx)
{
    McmAudioDemuxerContext *s = avctx->priv_data;
    mcm_audio_sampling mcm_sample_rate;
    mcm_conn_param param = { 0 };
    mcm_audio_ptime mcm_ptime;
    mcm_audio_format mcm_fmt;
    enum AVCodecID codec_id;
    AVStream *st;
    int err;

    err = mcm_parse_conn_param(avctx, &param, is_rx, s->ip_addr, s->port,
                               s->protocol_type, s->payload_type, s->socket_name,
                               s->interface_id);
    if (err)
        return err;

    /* payload type */
    if (param.payload_type != PAYLOAD_TYPE_ST30_AUDIO) {
        av_log(avctx, AV_LOG_ERROR, "Unknown payload type\n");
        return AVERROR(EINVAL);
    }

    err = mcm_parse_audio_sample_rate(avctx, &mcm_sample_rate, s->sample_rate);
    if (err)
        return err;

    err = mcm_parse_audio_packet_time(avctx, &mcm_ptime, s->ptime);
    if (err)
        return err;

    err = mcm_parse_audio_pcm_format(avctx, &mcm_fmt, &codec_id, s->pcm_format);
    if (err)
        return err;

    /* audio format */
    param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
    param.payload_args.audio_args.channel = s->channels;
    param.payload_args.audio_args.sampling = mcm_sample_rate;
    param.payload_args.audio_args.ptime = mcm_ptime;
    param.payload_args.audio_args.format = mcm_fmt;

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
    st->codecpar->codec_id = codec_id;
    st->codecpar->ch_layout.nb_channels = s->channels;
    st->codecpar->sample_rate = s->sample_rate;

    s->first_frame = true;

    av_log(avctx, AV_LOG_INFO,
           "codec:%s sampling:%d ch:%d ptime:%s\n",
           avcodec_get_name(codec_id), s->sample_rate, s->channels, s->ptime);
    return 0;
}

static int mcm_audio_read_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmAudioDemuxerContext *s = avctx->priv_data;
    mcm_buffer *buf = NULL;
    int timeout = s->first_frame ? -1 : 1000;
    int ret, len, err = 0;

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
    { "interface_id", "set interface id", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, DEC },
    { "channels", "number of audio channels", OFFSET(channels), AV_OPT_TYPE_INT, {.i64 = 2}, 1, INT_MAX, DEC },
    { "sample_rate", "audio sample rate", OFFSET(sample_rate), AV_OPT_TYPE_INT, {.i64 = 48000}, 1, INT_MAX, DEC },
    { "ptime", "audio packet time", OFFSET(ptime), AV_OPT_TYPE_STRING, {.str = "1ms"}, .flags = DEC },
    { "pcm_fmt", "audio PCM format", OFFSET(pcm_format), AV_OPT_TYPE_STRING, {.str = "pcm24"}, .flags = DEC },
    { NULL },
};

static const AVClass mcm_audio_demuxer_class = {
    .class_name = "mcm audio demuxer",
    .item_name = av_default_item_name,
    .option = mcm_audio_rx_options,
    .version = LIBAVUTIL_VERSION_INT,
    .category = AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT,
};

#ifdef MCM_FFMPEG_7_0
FFInputFormat ff_mcm_audio_demuxer = {
        .p.name = "mcm_audio",
        .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio"),
        .priv_data_size = sizeof(McmAudioDemuxerContext),
        .read_header = mcm_audio_read_header,
        .read_packet = mcm_audio_read_packet,
        .read_close = mcm_audio_read_close,
        .p.flags = AVFMT_NOFILE,
        .p.priv_class = &mcm_audio_demuxer_class,
};
#else /* MCM_FFMPEG_7_0 */
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
#endif /* MCM_FFMPEG_7_0 */
