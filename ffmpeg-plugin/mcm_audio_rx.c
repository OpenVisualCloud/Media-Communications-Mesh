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
#include <mesh_dp.h>
#ifdef MCM_FFMPEG_7_0
#include "libavformat/demux.h"
#endif /* MCM_FFMPEG_7_0 */

typedef struct McmAudioDemuxerContext {
    const AVClass *class; /**< Class for private options. */

    /* arguments */
    int conn_delay;
    char *conn_type;
    char *urn;
    char *ip_addr;
    int port;
    int payload_type;
    char *socket_name;
    int interface_id;

    int channels;
    int sample_rate;
    char* ptime;

    MeshClient *mc;
    MeshConnection *conn;
    bool first_frame;
} McmAudioDemuxerContext;

static int mcm_audio_read_header(AVFormatContext* avctx, enum AVCodecID codec_id)
{
    McmAudioDemuxerContext *s = avctx->priv_data;
    char json_config[1024];
    AVStream *st;
    int err, n;

    err = mcm_get_client(&s->mc);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Get mesh client failed: %s (%d)\n",
               mesh_err2str(err), err);
        return AVERROR(EINVAL);
    }

    if (!strcmp(s->conn_type, "multipoint-group")) {
        n = snprintf(json_config, sizeof(json_config),
                     mcm_json_config_multipoint_group_audio_format, s->conn_delay,
                     s->urn, s->channels, s->sample_rate,
                     avcodec_get_name(codec_id), s->ptime);
                     
    } else if (!strcmp(s->conn_type, "st2110")) {
        n = snprintf(json_config, sizeof(json_config),
                     mcm_json_config_st2110_audio_format, s->conn_delay,
                     s->ip_addr, s->port, s->payload_type,
                     s->channels, s->sample_rate,
                     avcodec_get_name(codec_id), s->ptime);
    } else {
        av_log(avctx, AV_LOG_ERROR, "Unknown conn type: '%s'\n", s->conn_type);
        return AVERROR(EINVAL);
    }

    av_log(avctx, AV_LOG_INFO, "JSON LEN = %d\n", n);
    mcm_replace_back_quotes(json_config);

    err = mesh_create_rx_connection(s->mc, &s->conn, json_config);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Create connection failed: %s (%d)\n",
               mesh_err2str(err), err);
        err = AVERROR(EIO);
        goto exit_put_client;
    }

    st = avformat_new_stream(avctx, NULL);
    if (!st) {
        mcm_put_client(&s->mc);
        err = AVERROR(ENOMEM);
        goto exit_delete_conn;
    }

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

exit_delete_conn:
    mesh_delete_connection(&s->conn);

exit_put_client:
    mcm_put_client(&s->mc);
    return err;
}

static int mcm_audio_read_header_pcm16(AVFormatContext* avctx)
{
    return mcm_audio_read_header(avctx, AV_CODEC_ID_PCM_S16BE);
}

static int mcm_audio_read_header_pcm24(AVFormatContext* avctx)
{
    return mcm_audio_read_header(avctx, AV_CODEC_ID_PCM_S24BE);
}

static int mcm_audio_read_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmAudioDemuxerContext *s = avctx->priv_data;
    int timeout = s->first_frame ? MESH_TIMEOUT_INFINITE : 1000;
    MeshBuffer *buf;
    int len, err;

    s->first_frame = false;

    err = mesh_get_buffer_timeout(s->conn, &buf, timeout);
    if (err == -MESH_ERR_CONN_CLOSED)
        return AVERROR_EOF;

    if (mcm_shutdown_requested())
        return AVERROR_EOF;

    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Get buffer error: %s (%d)\n",
               mesh_err2str(err), err);
        return AVERROR(EIO);
    }

    len = buf->data_len;
    err = av_new_packet(pkt, len);
    if (err)
        return err;

    memcpy(pkt->data, buf->data, len);

    pkt->pts = pkt->dts = AV_NOPTS_VALUE;

    err = mesh_put_buffer(&buf);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Put buffer error: %s (%d)\n",
               mesh_err2str(err), err);
        return AVERROR(EIO);
    }

    return len;
}

static int mcm_audio_read_close(AVFormatContext* avctx)
{
    McmAudioDemuxerContext* s = avctx->priv_data;
    int err;

    err = mesh_delete_connection(&s->conn);
    if (err)
        av_log(avctx, AV_LOG_ERROR, "Delete mesh connection failed: %s (%d)\n",
               mesh_err2str(err), err);

    err = mcm_put_client(&s->mc);
    if (err)
        av_log(avctx, AV_LOG_ERROR, "Put mesh client failed (%d)\n", err);

    return 0;
}

#define OFFSET(x) offsetof(McmAudioDemuxerContext, x)
#define DEC (AV_OPT_FLAG_DECODING_PARAM)
static const AVOption mcm_audio_rx_options[] = {
    { "conn_delay", "set connection creation delay", OFFSET(conn_delay), AV_OPT_TYPE_INT, {.i64 = 0}, 0, 10000, DEC },
    { "conn_type", "set connection type ('multipoint-group' or 'st2110')", OFFSET(conn_type), AV_OPT_TYPE_STRING, {.str = "multipoint-group"}, .flags = DEC },
    { "urn", "set multipoint group URN", OFFSET(urn), AV_OPT_TYPE_STRING, {.str = "192.168.97.1"}, .flags = DEC },
    { "ip_addr", "set ST2110 remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.1"}, .flags = DEC },
    { "port", "set ST2110 local port", OFFSET(port), AV_OPT_TYPE_INT, {.i64 = 9001}, 0, USHRT_MAX, DEC },
    { "payload_type", "set ST2110 payload type", OFFSET(payload_type), AV_OPT_TYPE_INT, {.i64 = 111}, 0, 127, DEC },
    { "socket_name", "set memif socket name", OFFSET(socket_name), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = DEC },
    { "interface_id", "set interface id", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, DEC },
    { "channels", "number of audio channels", OFFSET(channels), AV_OPT_TYPE_INT, {.i64 = 2}, 1, INT_MAX, DEC },
    { "sample_rate", "audio sample rate", OFFSET(sample_rate), AV_OPT_TYPE_INT, {.i64 = 48000}, 1, INT_MAX, DEC },
    { "ptime", "audio packet time", OFFSET(ptime), AV_OPT_TYPE_STRING, {.str = "1ms"}, .flags = DEC },
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
FFInputFormat ff_mcm_audio_pcm16_demuxer = {
        .p.name = "mcm_audio_pcm16",
        .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio pcm16"),
        .priv_data_size = sizeof(McmAudioDemuxerContext),
        .read_header = mcm_audio_read_header_pcm16,
        .read_packet = mcm_audio_read_packet,
        .read_close = mcm_audio_read_close,
        .p.flags = AVFMT_NOFILE,
        .p.priv_class = &mcm_audio_demuxer_class,
};

FFInputFormat ff_mcm_audio_pcm24_demuxer = {
        .p.name = "mcm_audio_pcm24",
        .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio pcm24"),
        .priv_data_size = sizeof(McmAudioDemuxerContext),
        .read_header = mcm_audio_read_header_pcm24,
        .read_packet = mcm_audio_read_packet,
        .read_close = mcm_audio_read_close,
        .p.flags = AVFMT_NOFILE,
        .p.priv_class = &mcm_audio_demuxer_class,
};
#else /* MCM_FFMPEG_7_0 */
AVInputFormat ff_mcm_audio_pcm16_demuxer = {
        .name = "mcm_audio_pcm16",
        .long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio pcm16"),
        .priv_data_size = sizeof(McmAudioDemuxerContext),
        .read_header = mcm_audio_read_header_pcm16,
        .read_packet = mcm_audio_read_packet,
        .read_close = mcm_audio_read_close,
        .flags = AVFMT_NOFILE,
        .priv_class = &mcm_audio_demuxer_class,
};

AVInputFormat ff_mcm_audio_pcm24_demuxer = {
        .name = "mcm_audio_pcm24",
        .long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio pcm24"),
        .priv_data_size = sizeof(McmAudioDemuxerContext),
        .read_header = mcm_audio_read_header_pcm24,
        .read_packet = mcm_audio_read_packet,
        .read_close = mcm_audio_read_close,
        .flags = AVFMT_NOFILE,
        .priv_class = &mcm_audio_demuxer_class,
};
#endif /* MCM_FFMPEG_7_0 */
