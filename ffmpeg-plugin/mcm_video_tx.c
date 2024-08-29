/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/internal.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libavformat/mux.h"
#include "libavdevice/mcm_common.h"
#include <mcm_dp.h>

typedef struct McmVideoMuxerContext {
    const AVClass *class; /**< Class for private options. */

    /* arguments */
    char *ip_addr;
    char *port;
    char *protocol_type;
    char *payload_type;
    char *socket_name;
    int interface_id;

    int width;
    int height;
    enum AVPixelFormat pixel_format;
    AVRational frame_rate;

    mcm_conn_context *tx_handle;
} McmVideoMuxerContext;

static int mcm_video_write_header(AVFormatContext* avctx)
{
    McmVideoMuxerContext *s = avctx->priv_data;
    mcm_conn_param param = { 0 };
    int err;

    err = mcm_parse_conn_param(avctx, &param, is_tx, s->ip_addr, s->port,
                               s->protocol_type, s->payload_type, s->socket_name,
                               s->interface_id);
    if (err)
        return err;

    switch (param.payload_type) {
    case PAYLOAD_TYPE_RTSP_VIDEO:
    case PAYLOAD_TYPE_ST20_VIDEO:
    case PAYLOAD_TYPE_ST22_VIDEO:
        /* video format */
        param.payload_args.video_args.width   = param.width = s->width;
        param.payload_args.video_args.height  = param.height = s->height;
        param.payload_args.video_args.fps     = param.fps = av_q2d(s->frame_rate);

        switch (s->pixel_format) {
        case AV_PIX_FMT_NV12:
            param.pix_fmt = PIX_FMT_NV12;
            break;
        case AV_PIX_FMT_YUV422P:
            param.pix_fmt = PIX_FMT_YUV422P;
            break;
        case AV_PIX_FMT_YUV444P10LE:
            param.pix_fmt = PIX_FMT_YUV444P_10BIT_LE;
            break;
        case AV_PIX_FMT_RGB24:
            param.pix_fmt = PIX_FMT_RGB8;
            break;
        case AV_PIX_FMT_YUV422P10LE:
        default:
            param.pix_fmt = PIX_FMT_YUV422P_10BIT_LE;
        }

        param.payload_args.video_args.pix_fmt = param.pix_fmt;
        break;

    default:
        av_log(avctx, AV_LOG_ERROR, "Unknown payload type\n");
        return AVERROR(EINVAL);
    }

    s->tx_handle = mcm_create_connection(&param);
    if (!s->tx_handle) {
        av_log(avctx, AV_LOG_ERROR, "Create connection failed\n");
        return AVERROR(EIO);
    }

    av_log(avctx, AV_LOG_INFO,
           "w:%d h:%d pixfmt:%s fps:%d\n",
           s->width, s->height, av_get_pix_fmt_name(s->pixel_format),
           (int)av_q2d(s->frame_rate));
    return 0;
}

static int mcm_video_write_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmVideoMuxerContext *s = avctx->priv_data;
    mcm_buffer *buf = NULL;
    int err = 0;

    buf = mcm_dequeue_buffer(s->tx_handle, -1, &err);
    if (buf == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Dequeue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    memcpy(buf->data, pkt->data, pkt->size <= buf->len ? pkt->size : buf->len);

    if ((err = mcm_enqueue_buffer(s->tx_handle, buf)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "Enqueue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    return 0;
}

static int mcm_video_write_trailer(AVFormatContext* avctx)
{
    McmVideoMuxerContext *s = avctx->priv_data;

    mcm_destroy_connection(s->tx_handle);
    return 0;
}

#define OFFSET(x) offsetof(McmVideoMuxerContext, x)
#define ENC AV_OPT_FLAG_ENCODING_PARAM
static const AVOption mcm_video_tx_options[] = {
    { "ip_addr", "set remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.2"}, .flags = ENC },
    { "port", "set remote port", OFFSET(port), AV_OPT_TYPE_STRING, {.str = "9001"}, .flags = ENC },
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = ENC },
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st20"}, .flags = ENC },
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
