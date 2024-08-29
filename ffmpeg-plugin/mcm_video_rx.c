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
#include "libavformat/internal.h"
#include "libavdevice/mcm_common.h"
#include <mcm_dp.h>
#ifdef MCM_FFMPEG_7_0
#include "libavformat/demux.h"
#endif /* MCM_FFMPEG_7_0 */

typedef struct McmVideoDemuxerContext {
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

    mcm_conn_context *rx_handle;
    bool first_frame;
    int frame_size;
} McmVideoDemuxerContext;

static int getFrameSize(video_pixel_format fmt, uint32_t width, uint32_t height, bool interlaced)
{
    size_t size = 0;
    size_t pixels = (size_t)(width*height);
    switch (fmt) {
        case PIX_FMT_YUV422P: /* YUV 422 packed 8bit(aka ST20_FMT_YUV_422_8BIT, aka ST_FRAME_FMT_UYVY) */
            size = pixels * 2;
            break;
        case PIX_FMT_RGB8:
            size = pixels * 3; /* 8 bits RGB pixel in a 24 bits (aka ST_FRAME_FMT_RGB8) */
            break;
/* Customized YUV 420 8bit, set transport format as ST20_FMT_YUV_420_8BIT. For direct transport of
none-RFC4175 formats like I420/NV12. When this input/output format is set, the frame is identical to
transport frame without conversion. The frame should not have lines padding) */
        case PIX_FMT_NV12: /* PIX_FMT_NV12, YUV 420 planar 8bits (aka ST_FRAME_FMT_YUV420CUSTOM8, aka ST_FRAME_FMT_YUV420PLANAR8) */
            size = pixels * 3 / 2;
            break;
        case PIX_FMT_YUV444P_10BIT_LE:
            size = pixels * 2 * 3;
            break;
        case PIX_FMT_YUV422P_10BIT_LE: /* YUV 422 planar 10bits little indian, in two bytes (aka ST_FRAME_FMT_YUV422PLANAR10LE) */
        default:
            size = pixels * 2 * 2;
            break;
    }
    if (interlaced) size /= 2; /* if all fmt support interlace? */
    return (int)size;
}

static int mcm_video_read_header(AVFormatContext* avctx)
{
    McmVideoDemuxerContext *s = avctx->priv_data;
    mcm_conn_param param = { 0 };
    AVStream *st;
    int err;

    err = mcm_parse_conn_param(avctx, &param, is_rx, s->ip_addr, s->port,
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

    s->frame_size = getFrameSize(param.pix_fmt, s->width, s->height, false);
    if (s->frame_size <= 0) {
        av_log(avctx, AV_LOG_ERROR, "calculate frame size failed\n");
        return AVERROR(EIO);
    }

    s->rx_handle = mcm_create_connection(&param);
    if (!s->rx_handle) {
        av_log(avctx, AV_LOG_ERROR, "create connection failed\n");
        return AVERROR(EIO);
    }

    st = avformat_new_stream(avctx, NULL);
    if (!st)
        return AVERROR(ENOMEM);

    avpriv_set_pts_info(st, 64, s->frame_rate.den, s->frame_rate.num);

    st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codecpar->codec_id   = AV_CODEC_ID_RAWVIDEO;
    st->codecpar->width      = s->width;
    st->codecpar->height     = s->height;
    st->codecpar->format     = s->pixel_format;

    s->first_frame = true;

    av_log(avctx, AV_LOG_INFO,
           "w:%d h:%d pixfmt:%s fps:%d\n",
           s->width, s->height, av_get_pix_fmt_name(s->pixel_format),
           (int)av_q2d(s->frame_rate));
    return 0;
}

static int mcm_video_read_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmVideoDemuxerContext *s = avctx->priv_data;
    mcm_buffer *buf = NULL;
    int timeout = s->first_frame ? -1 : 1000;
    int err = 0;
    int ret;

    s->first_frame = false;

    buf = mcm_dequeue_buffer(s->rx_handle, timeout, &err);
    if (buf == NULL) {
        if (!err)
            return AVERROR_EOF;

        av_log(avctx, AV_LOG_ERROR, "dequeue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    if ((ret = av_new_packet(pkt, s->frame_size)) < 0)
        return ret;

    memcpy(pkt->data, buf->data, FFMIN(s->frame_size, buf->len));

    pkt->pts = pkt->dts = AV_NOPTS_VALUE;

    if ((err = mcm_enqueue_buffer(s->rx_handle, buf)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "enqueue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    return s->frame_size;
}

static int mcm_video_read_close(AVFormatContext* avctx)
{
    McmVideoDemuxerContext* s = avctx->priv_data;

    mcm_destroy_connection(s->rx_handle);
    return 0;
}

#define OFFSET(x) offsetof(McmVideoDemuxerContext, x)
#define DEC (AV_OPT_FLAG_DECODING_PARAM)
static const AVOption mcm_video_rx_options[] = {
    { "ip_addr", "set remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.1"}, .flags = DEC },
    { "port", "set local port", OFFSET(port), AV_OPT_TYPE_STRING, {.str = "9001"}, .flags = DEC },
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = DEC },
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st20"}, .flags = DEC },
    { "socket_name", "set memif socket name", OFFSET(socket_name), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = DEC },
    { "interface_id", "set interface id", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, DEC },
    { "video_size", "set video frame size given a string such as 640x480 or hd720", OFFSET(width), AV_OPT_TYPE_IMAGE_SIZE, {.str = "1920x1080"}, 0, 0, DEC },
    { "pixel_format", "set video pixel format", OFFSET(pixel_format), AV_OPT_TYPE_PIXEL_FMT, {.i64 = AV_PIX_FMT_YUV422P10LE}, AV_PIX_FMT_NONE, INT_MAX, DEC },
    { "frame_rate", "set video frame rate", OFFSET(frame_rate), AV_OPT_TYPE_VIDEO_RATE, {.str = "25"}, 0, INT_MAX, DEC },
    { NULL },
};

static const AVClass mcm_video_demuxer_class = {
    .class_name = "mcm video demuxer",
    .item_name = av_default_item_name,
    .option = mcm_video_rx_options,
    .version = LIBAVUTIL_VERSION_INT,
    .category = AV_CLASS_CATEGORY_DEVICE_INPUT,
};

#ifdef MCM_FFMPEG_7_0
FFInputFormat ff_mcm_demuxer = {
        .p.name = "mcm",
        .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh video"),
        .priv_data_size = sizeof(McmVideoDemuxerContext),
        .read_header = mcm_video_read_header,
        .read_packet = mcm_video_read_packet,
        .read_close = mcm_video_read_close,
        .p.flags = AVFMT_NOFILE,
        .p.extensions = "mcm",
        .raw_codec_id = AV_CODEC_ID_RAWVIDEO,
        .p.priv_class = &mcm_video_demuxer_class,
};
#else /* MCM_FFMPEG_7_0 */
AVInputFormat ff_mcm_demuxer = {
        .name = "mcm",
        .long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh video"),
        .priv_data_size = sizeof(McmVideoDemuxerContext),
        .read_header = mcm_video_read_header,
        .read_packet = mcm_video_read_packet,
        .read_close = mcm_video_read_close,
        .flags = AVFMT_NOFILE,
        .extensions = "mcm",
        .raw_codec_id = AV_CODEC_ID_RAWVIDEO,
        .priv_class = &mcm_video_demuxer_class,
};
#endif /* MCM_FFMPEG_7_0 */
