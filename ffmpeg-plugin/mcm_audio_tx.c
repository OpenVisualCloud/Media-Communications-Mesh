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
#include <mesh_dp.h>
#include <unistd.h>

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

    MeshClient *mc;
    MeshConnection *conn;
    MeshBuffer *unsent_buf;
    int unsent_len;
} McmAudioMuxerContext;

static int mcm_audio_write_header(AVFormatContext* avctx)
{
    AVCodecParameters* codecpar = avctx->streams[0]->codecpar;
    McmAudioMuxerContext *s = avctx->priv_data;
    int kind = MESH_CONN_KIND_SENDER;
    MeshConfig_Audio cfg;
    int err;

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

    err = mcm_parse_audio_sample_rate(avctx, &cfg.sample_rate, s->sample_rate);
    if (err)
        return err;

    err = mcm_parse_audio_packet_time(avctx, &cfg.packet_time, s->ptime);
    if (err)
        return err;

    switch (codecpar->codec_id) {
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

    av_log(avctx, AV_LOG_INFO, "codec:%s sampling:%d ch:%d ptime:%s\n",
           avcodec_get_name(codecpar->codec_id), s->sample_rate, s->channels,
           s->ptime);
    return 0;

exit_delete_conn:
    mesh_delete_connection(&s->conn);

exit_put_client:
    mcm_put_client(&s->mc);
    return err;
}

static int mcm_audio_write_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmAudioMuxerContext *s = avctx->priv_data;
    const uint8_t *data = pkt->data;
    int size = pkt->size;
    int len, err = 0;

    while (size > 0) {
        size_t max_len;
        char *dest;

        if (!s->unsent_buf) {
            err = mesh_get_buffer(s->conn, &s->unsent_buf);

            if (err) {
                av_log(avctx, AV_LOG_ERROR, "Get buffer error: %s (%d)\n",
                       mesh_err2str(err), err);
                return AVERROR(EIO);
            }
        }

        max_len = s->unsent_buf->conn->buf_size;
        len = FFMIN(max_len, size + s->unsent_len);

        if (len < max_len) {
            memcpy((char *)s->unsent_buf->data + s->unsent_len, data, size);
            s->unsent_len += size;
            break;
        }

        dest = s->unsent_buf->data;

        if (s->unsent_len) {
            dest += s->unsent_len;
            len -= s->unsent_len;
            s->unsent_len = 0;
        }

        memcpy(dest, data, len);
        data += len;
        size -= len;

        err = mesh_put_buffer(&s->unsent_buf);
        if (err) {
            av_log(avctx, AV_LOG_ERROR, "Put buffer error: %s (%d)\n",
                   mesh_err2str(err), err);
            return AVERROR(EIO);
        }
    }

    return 0;
}

static int mcm_audio_write_trailer(AVFormatContext* avctx)
{
    McmAudioMuxerContext *s = avctx->priv_data;
    int err;

    if (s->unsent_buf) {
        /* zero the unused rest of the buffer */
        memset((char *)s->unsent_buf->data + s->unsent_len, 0,
               s->unsent_buf->data_len - s->unsent_len);

        err = mesh_put_buffer(&s->unsent_buf);
        if (err)
            av_log(avctx, AV_LOG_ERROR, "Enqueue buffer error: %s (%d)\n",
                   mesh_err2str(err), err);
    }

    err = mesh_delete_connection(&s->conn);
    if (err)
        av_log(avctx, AV_LOG_ERROR, "Delete mesh connection failed: %s (%d)\n",
               mesh_err2str(err), err);

    err = mcm_put_client(&s->mc);
    if (err)
        av_log(avctx, AV_LOG_ERROR, "Put mesh client failed (%d)\n", err);

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

const FFOutputFormat ff_mcm_audio_pcm24_muxer = {
    .p.name = "mcm_audio_pcm24",
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
