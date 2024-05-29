#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/internal.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libavformat/mux.h"
#include "libavformat/internal.h"
#include <mcm_dp.h>
#include <bsd/string.h>

typedef struct McmDemuxerContext {
    const AVClass *class; /**< Class for private options. */

    /* arguments */
    char *ip_addr;
    char *port;
    char *payload_type;
    char *protocol_type;
    int width;
    int height;
    enum AVPixelFormat pixel_format;
    AVRational frame_rate;
    char *socket_name;
    int interface_id;

    mcm_conn_context *rx_handle;
    bool first_frame;
} McmDemuxerContext;

static int mcm_read_header(AVFormatContext* avctx)
{
    McmDemuxerContext *s = avctx->priv_data;
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
    if (strcmp(s->payload_type, "st20") == 0) {
        param.payload_type = PAYLOAD_TYPE_ST20_VIDEO;
    } else if (strcmp(s->payload_type, "st22") == 0) {
        param.payload_type = PAYLOAD_TYPE_ST22_VIDEO;
    } else if (strcmp(s->payload_type, "st30") == 0) {
        param.payload_type = PAYLOAD_TYPE_ST30_AUDIO;
    } else if (strcmp(s->payload_type, "st40") == 0) {
        param.payload_type = PAYLOAD_TYPE_ST40_ANCILLARY;
    } else if (strcmp(s->payload_type, "rtsp") == 0) {
        param.payload_type = PAYLOAD_TYPE_RTSP_VIDEO;
    } else {
        av_log(avctx, AV_LOG_ERROR, "unknown payload type\n");
        return AVERROR(EINVAL);
    }

    switch (param.payload_type) {
    case PAYLOAD_TYPE_ST30_AUDIO:
        /* audio format */
        // param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
        // param.payload_args.audio_args.channel = 2;
        // param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
        // param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
        // param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;
        av_log(avctx, AV_LOG_ERROR, "payload type st30 is not yet supported\n");
        return AVERROR(EINVAL); // not supported yet

    case PAYLOAD_TYPE_ST40_ANCILLARY:
        /* ancillary format */
        // param.payload_args.anc_args.format = ANC_FORMAT_CLOSED_CAPTION;
        // param.payload_args.anc_args.type = ANC_TYPE_FRAME_LEVEL;
        // param.payload_args.anc_args.fps = av_q2d(s->frame_rate);
        av_log(avctx, AV_LOG_ERROR, "payload type st40 is not yet supported\n");
        return AVERROR(EINVAL); // not supported yet

    case PAYLOAD_TYPE_RTSP_VIDEO:
    case PAYLOAD_TYPE_ST20_VIDEO:
    case PAYLOAD_TYPE_ST22_VIDEO:
    default:
        /* video format */
        param.payload_args.video_args.width   = param.width = s->width;
        param.payload_args.video_args.height  = param.height = s->height;
        param.payload_args.video_args.fps     = param.fps = av_q2d(s->frame_rate);

        switch (s->pixel_format) {
        case AV_PIX_FMT_YUV420P:
        default:
            param.pix_fmt = PIX_FMT_NV12;
        }

        param.payload_args.video_args.pix_fmt = param.pix_fmt;
        break;
    }

    param.type = is_rx;

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

static int mcm_read_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmDemuxerContext *s = avctx->priv_data;
    mcm_buffer *buf = NULL;
    int timeout = s->first_frame ? -1 : 1000;
    int err = 0;
    int frame_size;
    int ret;

    s->first_frame = false;

    buf = mcm_dequeue_buffer(s->rx_handle, timeout, &err);
    if (buf == NULL) {
        if (!err)
            return AVERROR_EOF;

        av_log(avctx, AV_LOG_ERROR, "dequeue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    frame_size = (s->width * s->height * 3)/2;

    if ((ret = av_new_packet(pkt, frame_size)) < 0)
        return ret;

    memcpy(pkt->data, buf->data, frame_size);

    pkt->pts = pkt->dts = AV_NOPTS_VALUE;

    if ((err = mcm_enqueue_buffer(s->rx_handle, buf)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "enqueue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    return frame_size;
}

static int mcm_read_close(AVFormatContext* avctx)
{
    McmDemuxerContext* s = avctx->priv_data;

    mcm_destroy_connection(s->rx_handle);
    return 0;
}

#define OFFSET(x) offsetof(McmDemuxerContext, x)
#define DEC (AV_OPT_FLAG_DECODING_PARAM)
static const AVOption mcm_rx_options[] = {
    { "ip_addr", "set remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.1"}, .flags = DEC },
    { "port", "set local port", OFFSET(port), AV_OPT_TYPE_STRING, {.str = "9001"}, .flags = DEC },
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st20"}, .flags = DEC },
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = DEC },
    { "video_size", "set video frame size given a string such as 640x480 or hd720", OFFSET(width), AV_OPT_TYPE_IMAGE_SIZE, {.str = "1920x1080"}, 0, 0, DEC },
    { "pixel_format", "set video pixel format", OFFSET(pixel_format), AV_OPT_TYPE_PIXEL_FMT, {.i64 = AV_PIX_FMT_YUV420P}, AV_PIX_FMT_NONE, INT_MAX, DEC },
    { "frame_rate", "set video frame rate", OFFSET(frame_rate), AV_OPT_TYPE_VIDEO_RATE, {.str = "25"}, 0, INT_MAX, DEC },
    { "socket_name", "set memif socket name", OFFSET(socket_name), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = DEC },
    { "interface_id", "set interface ID", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, DEC },
    { NULL },
};

static const AVClass mcm_demuxer_class = {
    .class_name = "mcm demuxer",
    .item_name = av_default_item_name,
    .option = mcm_rx_options,
    .version = LIBAVUTIL_VERSION_INT,
    .category = AV_CLASS_CATEGORY_DEVICE_INPUT,
};

AVInputFormat ff_mcm_demuxer = {
        .name = "mcm",
        .long_name = NULL_IF_CONFIG_SMALL("Media Communication Mesh"),
        .priv_data_size = sizeof(McmDemuxerContext),
        .read_header = mcm_read_header,
        .read_packet = mcm_read_packet,
        .read_close = mcm_read_close,
        .flags = AVFMT_NOFILE,
        .extensions = "mcm",
        .raw_codec_id = AV_CODEC_ID_RAWVIDEO,
        .priv_class = &mcm_demuxer_class,
};
