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
#include "libavutil/pixdesc.h"
#include "libavdevice/mcm_common.h"

typedef struct McmVideoMuxerContext {
    const AVClass *class; /**< Class for private options. */

    /* arguments */
    char *conn_type;
    char *urn;
    char *ip_addr;
    int port;
    char *transport;
    char *transport_pixel_format;
    char *socket_name;
    int interface_id;

    int width;
    int height;
    enum AVPixelFormat pixel_format;
    AVRational frame_rate;

    MeshClient *mc;
    MeshConnection *conn;
} McmVideoMuxerContext;

static int mcm_video_write_header(AVFormatContext* avctx)
{
    McmVideoMuxerContext *s = avctx->priv_data;
    char json_config[250];
    int err, n;

    err = mcm_get_client(&s->mc);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Get mesh client failed: %s (%d)\n",
               mesh_err2str(err), err);
        return AVERROR(EINVAL);
    }

    if (!strcmp(s->conn_type, "multipoint-group")) {
        n = snprintf(json_config, sizeof(json_config),
                     mcm_json_config_multipoint_group_video_format, s->urn,
                     s->width, s->height, av_q2d(s->frame_rate),
                     av_get_pix_fmt_name(s->pixel_format));
    } else if (!strcmp(s->conn_type, "st2110")) {
        n = snprintf(json_config, sizeof(json_config),
                     mcm_json_config_st2110_video_format,
                     s->ip_addr, s->port, s->transport, s->transport_pixel_format,
                     s->width, s->height, av_q2d(s->frame_rate),
                     av_get_pix_fmt_name(s->pixel_format));
    } else {
        av_log(avctx, AV_LOG_ERROR, "Unknown conn type: '%s'\n", s->conn_type);
        return AVERROR(EINVAL);
    }

    av_log(avctx, AV_LOG_INFO, "JSON LEN = %d\n", n);
    mcm_replace_back_quotes(json_config);

    err = mesh_create_tx_connection(s->mc, &s->conn, json_config);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Create connection failed: %s (%d)\n",
               mesh_err2str(err), err);
        err = AVERROR(EIO);
        goto exit_put_client;
    }

    av_log(avctx, AV_LOG_INFO,
           "w:%d h:%d pixfmt:%s fps:%d\n",
           s->width, s->height, av_get_pix_fmt_name(s->pixel_format),
           (int)av_q2d(s->frame_rate));
    return 0;

exit_put_client:
    mcm_put_client(&s->mc);
    return err;
}

static int mcm_video_write_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmVideoMuxerContext *s = avctx->priv_data;
    MeshBuffer *buf;
    int err;

    err = mesh_get_buffer(s->conn, &buf);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Get buffer error: %s (%d)\n",
               mesh_err2str(err), err);
        return AVERROR(EIO);
    }

    memcpy(buf->data, pkt->data,
           pkt->size <= buf->data_len ? pkt->size : buf->data_len);

    err = mesh_put_buffer(&buf);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Put buffer error: %s (%d)\n",
                mesh_err2str(err), err);
        return AVERROR(EIO);
    }

    return 0;
}

static int mcm_video_write_trailer(AVFormatContext* avctx)
{
    McmVideoMuxerContext *s = avctx->priv_data;
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

#define OFFSET(x) offsetof(McmVideoMuxerContext, x)
#define ENC AV_OPT_FLAG_ENCODING_PARAM
static const AVOption mcm_video_tx_options[] = {
    { "conn_type", "set connection type ('multipoint-group' or 'st2110')", OFFSET(conn_type), AV_OPT_TYPE_STRING, {.str = "multipoint-group"}, .flags = ENC },
    { "urn", "set multipoint group URN", OFFSET(urn), AV_OPT_TYPE_STRING, {.str = "192.168.97.1"}, .flags = ENC },
    { "ip_addr", "set ST2110 remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.2"}, .flags = ENC },
    { "port", "set ST2110 local port", OFFSET(port), AV_OPT_TYPE_INT, {.i64 = 9001}, 0, USHRT_MAX, ENC },
    { "transport", "set ST2110 transport type", OFFSET(transport), AV_OPT_TYPE_STRING, {.str = "st2110-20"}, .flags = ENC },
    { "transport_pixel_format", "set st2110-20 transport pixel format", OFFSET(transport_pixel_format), AV_OPT_TYPE_STRING, {.str = "yuv422p10rfc4175"}, .flags = ENC },
    { "socket_name", "set memif socket name", OFFSET(socket_name), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = ENC },
    { "interface_id", "set interface id", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, ENC },
    { "video_size", "set video frame size given a string such as 640x480 or hd720", OFFSET(width), AV_OPT_TYPE_IMAGE_SIZE, {.str = "1920x1080"}, 0, 0, ENC },
    { "pixel_format", "set video pixel format", OFFSET(pixel_format), AV_OPT_TYPE_PIXEL_FMT, {.i64 = AV_PIX_FMT_YUV422P10LE}, AV_PIX_FMT_NONE, INT_MAX, ENC },
    { "frame_rate", "set video frame rate", OFFSET(frame_rate), AV_OPT_TYPE_VIDEO_RATE, {.str = "25"}, 0, INT_MAX, ENC },
    { NULL },
};

static const AVClass mcm_video_muxer_class = {
    .class_name = "mcm video muxer",
    .item_name = av_default_item_name,
    .option = mcm_video_tx_options,
    .version = LIBAVUTIL_VERSION_INT,
    .category = AV_CLASS_CATEGORY_DEVICE_OUTPUT,
};

const FFOutputFormat ff_mcm_muxer = {
    .p.name = "mcm",
    .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh video"),
    .priv_data_size = sizeof(McmVideoMuxerContext),
    .write_header = mcm_video_write_header,
    .write_packet = mcm_video_write_packet,
    .write_trailer = mcm_video_write_trailer,
    .p.video_codec = AV_CODEC_ID_RAWVIDEO,
    .p.flags = AVFMT_NOFILE,
    .p.priv_class = &mcm_video_muxer_class,
};
