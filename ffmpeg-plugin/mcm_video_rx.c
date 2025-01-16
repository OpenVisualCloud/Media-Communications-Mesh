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
#include "libavutil/pixdesc.h"
#include "libavdevice/mcm_common.h"
#ifdef MCM_FFMPEG_7_0
#include "libavformat/demux.h"
#endif /* MCM_FFMPEG_7_0 */

typedef struct McmVideoDemuxerContext {
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
    bool first_frame;
} McmVideoDemuxerContext;

static int mcm_video_read_header(AVFormatContext* avctx)
{
    McmVideoDemuxerContext *s = avctx->priv_data;
    char json_config[250];
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

    av_log(avctx, AV_LOG_DEBUG, "JSON LEN = %d", n);
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
        err = AVERROR(ENOMEM);
        goto exit_delete_conn;
    }

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

exit_delete_conn:
    mesh_delete_connection(&s->conn);

exit_put_client:
    mcm_put_client(&s->mc);
    return err;
}

static int mcm_video_read_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmVideoDemuxerContext *s = avctx->priv_data;
    MeshBuffer *buf;
    int timeout = s->first_frame ? MESH_TIMEOUT_INFINITE : 1000;
    int err, ret;

    s->first_frame = false;

    err = mesh_get_buffer_timeout(s->conn, &buf, timeout);
    if (err == -MESH_ERR_CONN_CLOSED)
        return AVERROR_EOF;

    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Get buffer error: %s (%d)\n",
               mesh_err2str(err), err);
        return AVERROR(EIO);
    }

    if ((ret = av_new_packet(pkt, s->conn->buf_size)) < 0)
        return ret;

    memcpy(pkt->data, buf->data, FFMIN(s->conn->buf_size, buf->data_len));

    pkt->pts = pkt->dts = AV_NOPTS_VALUE;

    err = mesh_put_buffer(&buf);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Put buffer error: %s (%d)\n",
               mesh_err2str(err), err);
        return AVERROR(EIO);
    }

    return s->conn->buf_size;
}

static int mcm_video_read_close(AVFormatContext* avctx)
{
    McmVideoDemuxerContext* s = avctx->priv_data;
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

#define OFFSET(x) offsetof(McmVideoDemuxerContext, x)
#define DEC (AV_OPT_FLAG_DECODING_PARAM)
static const AVOption mcm_video_rx_options[] = {
    { "conn_type", "set connection type ('multipoint-group' or 'st2110')", OFFSET(conn_type), AV_OPT_TYPE_STRING, {.str = "multipoint-group"}, .flags = DEC },
    { "urn", "set multipoint group URN", OFFSET(urn), AV_OPT_TYPE_STRING, {.str = "192.168.97.1"}, .flags = DEC },
    { "ip_addr", "set ST2110 remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.1"}, .flags = DEC },
    { "port", "set ST2110 local port", OFFSET(port), AV_OPT_TYPE_INT, {.i64 = 9001}, 0, USHRT_MAX, DEC },
    { "transport", "set ST2110 transport type", OFFSET(transport), AV_OPT_TYPE_STRING, {.str = "st2110-20"}, .flags = DEC },
    { "transport_pixel_format", "set st2110-20 transport pixel format", OFFSET(transport_pixel_format), AV_OPT_TYPE_STRING, {.str = "yuv422p10rfc4175"}, .flags = DEC },
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
