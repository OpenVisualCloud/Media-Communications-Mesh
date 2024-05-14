
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/internal.h"
#include "libavformat/avformat.h"
#include "libavformat/mux.h"
#include "libavformat/internal.h"
#include <mcm_dp.h>
#include <bsd/string.h>

typedef struct McmDemuxerContext {
    const AVClass* class; /**< Class for private options. */

    int idx;
    /* arguments */
    char* ip_addr;
    int port;
    int total_frame_num;
    char* payload_type;
    char* protocol_type;
    char* socket_path;
    uint8_t is_master;
    uint32_t interface_id;

    int width;
    int height;
    enum AVPixelFormat pixel_format;
    int fps;
    uint32_t frame_size;

    mcm_conn_context *rx_handle;
    uint64_t frame_count;
    bool first_frame;

} McmDemuxerContext;

static int mcm_read_header(AVFormatContext* avctx) {
    AVStream *st;

    av_log(NULL, AV_LOG_ERROR, "MCM READ HEADER\n");

    st = avformat_new_stream(avctx, NULL);
    if (!st)
        return AVERROR(ENOMEM);

    avpriv_set_pts_info(st, 64, 1, 1000000); /* 64 bits pts in microseconds */

    st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO; // AVMEDIA_TYPE_DATA;
    st->codecpar->codec_id   = AV_CODEC_ID_RAWVIDEO; // AV_CODEC_ID_VNULL; // AV_CODEC_ID_NONE; // AV_CODEC_ID_RAWVIDEO;

    st->codecpar->width      = 15;
    st->codecpar->height     = 12;
    st->codecpar->format     = AV_PIX_FMT_YUV422P10LE; // AV_PIX_FMT_YUV420P; // pix_fmt;
    st->avg_frame_rate       = av_make_q(10, 10);
    st->codecpar->bit_rate   = 1000000;
    st->duration = 30000;

    // McmDemuxerContext* s = avctx->priv_data;
    // mcm_conn_param param = { 0 };

    // s->frame_size = s->width * s->height * 3 / 2; //TODO:assume it's NV12
    // s->first_frame =true;

    //  param.type = is_rx;

    // /* protocol type */

    // if (strncmp(s->protocol_type, "memif", sizeof(s->protocol_type)) == 0) {
    //     param.protocol = PROTO_MEMIF;
    //     strlcpy(param.memif_interface.socket_path, s->socket_path, sizeof(param.memif_interface.socket_path));
    //     param.memif_interface.is_master = s->is_master;
    //     param.memif_interface.interface_id = s->interface_id;
    // } else if (strncmp(s->protocol_type, "udp", sizeof(s->protocol_type)) == 0) {
    //     param.protocol = PROTO_UDP;
    // } else if (strncmp(s->protocol_type, "tcp", sizeof(s->protocol_type)) == 0) {
    //     param.protocol = PROTO_TCP;
    // } else if (strncmp(s->protocol_type, "http", sizeof(s->protocol_type)) == 0) {
    //     param.protocol = PROTO_HTTP;
    // } else if (strncmp(s->protocol_type, "grpc", sizeof(s->protocol_type)) == 0) {
    //     param.protocol = PROTO_GRPC;
    // } else {
    //     param.protocol = PROTO_AUTO;
    // }

    // /* payload type */
    // if (strncmp(s->payload_type, "st20", sizeof(s->payload_type)) == 0) {
    //     param.payload_type = PAYLOAD_TYPE_ST20_VIDEO;
    // } else if (strncmp(s->payload_type, "st22", sizeof(s->payload_type)) == 0) {
    //     param.payload_type = PAYLOAD_TYPE_ST22_VIDEO;
    // } else if (strncmp(s->payload_type, "st30", sizeof(s->payload_type)) == 0) {
    //     param.payload_type = PAYLOAD_TYPE_ST30_AUDIO;
    // } else if (strncmp(s->payload_type, "st40", sizeof(s->payload_type)) == 0) {
    //     param.payload_type = PAYLOAD_TYPE_ST40_ANCILLARY;
    // } else if (strncmp(s->payload_type, "rtsp", sizeof(s->payload_type)) == 0) {
    //     param.payload_type = PAYLOAD_TYPE_RTSP_VIDEO;
    // } else {
    //     param.payload_type = PAYLOAD_TYPE_NONE;
    // }

    // switch (param.payload_type) {
    // case PAYLOAD_TYPE_ST30_AUDIO:
    //     /* audio format */
    //     param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
    //     param.payload_args.audio_args.channel = 2;
    //     param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
    //     param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
    //     param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;
    //     break;
    // case PAYLOAD_TYPE_ST40_ANCILLARY:
    //     /* ancillary format */
    //     param.payload_args.anc_args.format = ANC_FORMAT_CLOSED_CAPTION;
    //     param.payload_args.anc_args.type = ANC_TYPE_FRAME_LEVEL;
    //     param.payload_args.anc_args.fps = s->fps;
    //     break;
    // case PAYLOAD_TYPE_RTSP_VIDEO:
    // case PAYLOAD_TYPE_ST20_VIDEO:
    // case PAYLOAD_TYPE_ST22_VIDEO:
    // default:
    //     /* video format */
    //     param.pix_fmt = PIX_FMT_YUV444M;
    //     param.payload_args.video_args.width   = param.width = s->width;
    //     param.payload_args.video_args.height  = param.height = s->height;
    //     param.payload_args.video_args.fps     = param.fps = s->fps;
    //     param.payload_args.video_args.pix_fmt = param.pix_fmt = s->pixel_format;
    //     break;
    // }

    // strlcpy(param.remote_addr.ip, s->ip_addr, sizeof(param.remote_addr.ip));
    // strlcpy(param.local_addr.port, s->port, sizeof(param.local_addr.port));

    // s->rx_handle = mcm_create_connection(&param);
    // if (!s->rx_handle) {
    //     //err(avctx, "%s, mcm_create_connection failed\n", __func__);
    //     return AVERROR(EIO);
    // }
    return 0;
}

static int total=105000;
static int cnt=0;

static int mcm_read_packet(AVFormatContext* avctx, AVPacket* pkt) {
    int size = 10000 <= total ? 10000 : total;
    int ret;

    total -= size;

    av_log(NULL, AV_LOG_ERROR, "MCM READ PACKET %d (%d)\n", size, cnt);

    if (size <= 0)
        return AVERROR_EOF;

    if ((ret = av_new_packet(pkt, size)) < 0)
        return ret;

    pkt->pts = pkt->dts = cnt++;
    pkt->data[0] = 0xAA;
    pkt->data[1] = 0x77;
    pkt->data[2] = 0x13 + cnt;

    // McmDemuxerContext* s = avctx->priv_data;
    // mcm_buffer* buf = NULL;
    // int timeout = -1;
    // int err = 0;

    // /* receive frame */
    // if (s->first_frame) {
    //     /* infinity for the 1st frame. */
    //     timeout = -1;
    //     s->first_frame = false;
    // } else {
    //     /* 1 second */
    //     timeout = 1000;
    // }

    // buf = mcm_dequeue_buffer(s->rx_handle, timeout, &err);

    // if (buf == NULL) {
    //     return AVERROR(EIO);
    // }

    // if (strncmp(s->payload_type, "rtsp", sizeof(s->payload_type)) != 0) {
    //     fwrite(buf->data, s->frame_size, 1, pkt->data);
    // } else { //TODO: rtsp receiver side test code
    //     fwrite(buf->data, buf->len, 1, pkt->data);
    // }

    // s->frame_count++;

    // /* enqueue buffer */
    // if (mcm_enqueue_buffer(s->rx_handle, buf) != 0) {
    //     return AVERROR(EIO);
    // }
    return size;
}

static int mcm_read_close(AVFormatContext* avctx) {

    av_log(NULL, AV_LOG_ERROR, "MCM READ CLOSE\n");

    // McmDemuxerContext* s = avctx->priv_data;

    // mcm_destroy_connection(s->rx_handle);
    return 0;
}

#define OFFSET(x) offsetof(McmDemuxerContext, x)
#define DEC (AV_OPT_FLAG_DECODING_PARAM)
static const AVOption mcm_rx_options[] = {
    {"ip_addr", "IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = DEC},
    {"port", "port", OFFSET(port), AV_OPT_TYPE_INT, {.i64 = 9001}, -1, INT_MAX, DEC},
    {"fps", "FPS", OFFSET(fps), AV_OPT_TYPE_INT, {.i64 = 30}, -1, INT_MAX, DEC},
    {"total_frame_num", "Frame number", OFFSET(total_frame_num), AV_OPT_TYPE_INT, {.i64 = 10000}, -1, INT_MAX, DEC},
    {"payload_type", "Payload type", OFFSET(payload_type), AV_OPT_TYPE_INT, {.str = NULL}, .flags = DEC},
    // {"protocol_type", "Protocol type", OFFSET(protocol_type), AV_OPT_TYPE_INT, {.str = "auto"}, .flags = DEC},
    {"is_master", "Is master", OFFSET(is_master), AV_OPT_TYPE_INT, {.i64 = 1}, -1, INT_MAX, DEC},
    {"interface_id", "interface ID", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, DEC},
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
        .long_name = NULL_IF_CONFIG_SMALL("Media Communication Mesh input device"),
        .priv_data_size = sizeof(McmDemuxerContext),
        .read_header = mcm_read_header,
        .read_packet = mcm_read_packet,
        .read_close = mcm_read_close,
        .flags = AVFMT_NOFILE,
        .extensions = "mcm",
        .raw_codec_id = AV_CODEC_ID_RAWVIDEO, // AV_CODEC_ID_VNULL, // AV_CODEC_ID_NONE, // AV_CODEC_ID_RAWVIDEO,
        .priv_class = &mcm_demuxer_class,
};
