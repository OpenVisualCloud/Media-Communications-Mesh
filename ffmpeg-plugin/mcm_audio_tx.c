#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/internal.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libavformat/mux.h"
#include <mcm_dp.h>
#include <bsd/string.h>

typedef struct McmAudioMuxerContext {
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

    mcm_conn_context *tx_handle;
} McmAudioMuxerContext;

static int mcm_audio_write_header(AVFormatContext* avctx)
{
    McmAudioMuxerContext *s = avctx->priv_data;
    mcm_conn_param param = { 0 };

    strlcpy(param.remote_addr.ip, s->ip_addr, sizeof(param.remote_addr.ip));
    strlcpy(param.remote_addr.port, s->port, sizeof(param.remote_addr.port));

    /* protocol type */
    if (strcmp(s->protocol_type, "memif") == 0) {
        param.protocol = PROTO_MEMIF;
        param.memif_interface.is_master = 1;
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
        param.payload_codec = PAYLOAD_CODEC_JPEGXS;
    } else if (strcmp(s->payload_type, "st30") == 0) {
        param.payload_type = PAYLOAD_TYPE_ST30_AUDIO;
    } else if (strcmp(s->payload_type, "st40") == 0) {
        param.payload_type = PAYLOAD_TYPE_ST40_ANCILLARY;
    } else if (strcmp(s->payload_type, "rtsp") == 0) {
        param.payload_type = PAYLOAD_TYPE_RTSP_VIDEO;
    } else {
        av_log(NULL, AV_LOG_ERROR, "Unknown payload type\n");
        return AVERROR(EINVAL);
    }

    switch (param.payload_type) {
    case PAYLOAD_TYPE_ST30_AUDIO:
        /* audio format */
        param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
        param.payload_args.audio_args.channel = 2;
        param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
        param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
        param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;
        break;

    case PAYLOAD_TYPE_ST40_ANCILLARY:
        /* ancillary format */
        // param.payload_args.anc_args.format = ANC_FORMAT_CLOSED_CAPTION;
        // param.payload_args.anc_args.type = ANC_TYPE_FRAME_LEVEL;
        // param.payload_args.anc_args.fps = av_q2d(s->frame_rate);
        av_log(NULL, AV_LOG_ERROR, "Payload type st40 is not yet supported\n");
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
    }

    param.type = is_tx;

    s->tx_handle = mcm_create_connection(&param);
    if (!s->tx_handle) {
        av_log(avctx, AV_LOG_ERROR, "create connection failed\n");
        return AVERROR(EIO);
    }

    av_log(avctx, AV_LOG_INFO,
           "w:%d h:%d pixfmt:%s fps:%d\n",
           s->width, s->height, av_get_pix_fmt_name(s->pixel_format),
           (int)av_q2d(s->frame_rate));
    return 0;
}

static int mcm_audio_write_packet(AVFormatContext* avctx, AVPacket* pkt)
{
    McmAudioMuxerContext *s = avctx->priv_data;
    mcm_buffer *buf = NULL;
    int err = 0;

    buf = mcm_dequeue_buffer(s->tx_handle, -1, &err);
    if (buf == NULL) {
        av_log(avctx, AV_LOG_ERROR, "dequeue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    memcpy(buf->data, pkt->data, pkt->size <= buf->len ? pkt->size : buf->len);

    if ((err = mcm_enqueue_buffer(s->tx_handle, buf)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "enqueue buffer error %d\n", err);
        return AVERROR(EIO);
    }

    return 0;
}

static int mcm_audio_write_trailer(AVFormatContext* avctx)
{
    McmAudioMuxerContext *s = avctx->priv_data;

    mcm_destroy_connection(s->tx_handle);
    return 0;
}

#define OFFSET(x) offsetof(McmAudioMuxerContext, x)
#define ENC AV_OPT_FLAG_ENCODING_PARAM
static const AVOption mcm_audio_tx_options[] = {
    { "ip_addr", "set remote IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = "192.168.96.2"}, .flags = ENC },
    { "port", "set remote port", OFFSET(port), AV_OPT_TYPE_STRING, {.str = "9001"}, .flags = ENC },
    { "payload_type", "set payload type", OFFSET(payload_type), AV_OPT_TYPE_STRING, {.str = "st20"}, .flags = ENC },
    { "protocol_type", "set protocol type", OFFSET(protocol_type), AV_OPT_TYPE_STRING, {.str = "auto"}, .flags = ENC },
    { "video_size", "set video frame size given a string such as 640x480 or hd720", OFFSET(width), AV_OPT_TYPE_IMAGE_SIZE, {.str = "1920x1080"}, 0, 0, ENC },
    { "pixel_format", "set video pixel format", OFFSET(pixel_format), AV_OPT_TYPE_PIXEL_FMT, {.i64 = AV_PIX_FMT_YUV422P10LE}, AV_PIX_FMT_NONE, INT_MAX, ENC },
    { "frame_rate", "set video frame rate", OFFSET(frame_rate), AV_OPT_TYPE_VIDEO_RATE, {.str = "25"}, 0, INT_MAX, ENC },
    { "socket_name", "set memif socket name", OFFSET(socket_name), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = ENC },
    { "interface_id", "set interface ID", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, ENC },
    { NULL },
};

static const AVClass mcm_audio_muxer_class = {
    .class_name = "mcm audio muxer",
    .item_name = av_default_item_name,
    .option = mcm_audio_tx_options,
    .version = LIBAVUTIL_VERSION_INT,
    .category = AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT,
};

const FFOutputFormat ff_mcm_audio_muxer = {
    .p.name = "mcm_audio",
    .p.long_name = NULL_IF_CONFIG_SMALL("Media Communications Mesh audio"),
    .priv_data_size = sizeof(McmAudioMuxerContext),
    .write_header = mcm_audio_write_header,
    .write_packet = mcm_audio_write_packet,
    .write_trailer = mcm_audio_write_trailer,
    .p.audio_codec = AV_NE(AV_CODEC_ID_PCM_S16BE, AV_CODEC_ID_PCM_S16LE),
    .p.flags = AVFMT_NOFILE,
    .p.priv_class = &mcm_audio_muxer_class,
};
