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
    char *ip_addr;
    char *port;
    char *protocol_type;
    char *payload_type;
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
    int kind = MESH_CONN_KIND_RECEIVER;
    MeshConfig_Audio cfg;
    AVStream *st;
    int err;

    err = mcm_parse_audio_sample_rate(avctx, &cfg.sample_rate, s->sample_rate);
    if (err)
        return err;

    err = mcm_parse_audio_packet_time(avctx, &cfg.packet_time, s->ptime);
    if (err)
        return err;

    /* audio format */
    switch (codec_id) {
    case AV_CODEC_ID_PCM_S24BE:
        cfg.format = MESH_AUDIO_FORMAT_PCM_S24BE;
        break;
    case AV_CODEC_ID_PCM_S16BE:
        cfg.format = MESH_AUDIO_FORMAT_PCM_S16BE;
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Audio PCM format not supported\n");
        return AVERROR(EINVAL);
    }

    cfg.channels = s->channels;

    err = mcm_get_client(&s->mc);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Get mesh client failed: %s (%d)\n",
               mesh_err2str(err), err);
        return AVERROR(EINVAL);
    }

    err = mesh_create_connection(s->mc, &s->conn);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Create connection failed: %s (%d)\n",
               mesh_err2str(err), err);
        err = AVERROR(EIO);
        goto exit_put_client;
    }

    err = mcm_parse_conn_param(avctx, s->conn, kind, s->ip_addr, s->port,
                                  s->protocol_type, s->payload_type,
                                  s->socket_name, s->interface_id);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Configuration parsing failed: %s (%d)\n",
               mesh_err2str(err), err);
        err = AVERROR(EINVAL);
        goto exit_delete_conn;
    }

    err = mesh_apply_connection_config_audio(s->conn, &cfg);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Failed to apply connection config: %s (%d)\n",
               mesh_err2str(err), err);
        err = AVERROR(EIO);
        goto exit_delete_conn;
    }

    err = mesh_establish_connection(s->conn, kind);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Cannot establish connection: %s (%d)\n",
               mesh_err2str(err), err);
        err = AVERROR(EIO);
        goto exit_delete_conn;
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
    { "ip_addr", "set remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.1"}, .flags = DEC },
    { "port", "set local port", OFFSET(port), AV_OPT_TYPE_STRING, {.str = "9001"}, .flags = DEC },
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = DEC },
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st30"}, .flags = DEC },
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
