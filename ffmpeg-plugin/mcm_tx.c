#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/internal.h"
#include "libavformat/avformat.h"
#include "libavformat/mux.h"
#include <mcm_dp.h>
#include <bsd/string.h>

#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"

typedef struct McmMuxerContext {
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

    mcm_conn_context *tx_handle;
    uint64_t frame_count;

} McmMuxerContext;

static int mcm_write_header(AVFormatContext* ctx) {

    av_log(NULL, AV_LOG_WARNING, "MCM WRITE HEADER %d %d\n", ctx->streams[0]->codecpar->width, ctx->streams[0]->codecpar->height);

    // McmMuxerContext* s = ctx->priv_data;
    // mcm_conn_param param = { 0 };

    // char socket_path[108] = DEFAULT_MEMIF_SOCKET_PATH;

    // param.type = is_tx;
    // param.width = s->width = ctx->streams[0]->codecpar->width;
    // param.height = s->height = ctx->streams[0]->codecpar->height;


    // if (strncmp(s->protocol_type, "memif", sizeof(s->protocol_type)) == 0) {
    //     param.protocol = PROTO_MEMIF;
    //     strlcpy(param.memif_interface.socket_path, socket_path, sizeof(param.memif_interface.socket_path));
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
    //     case PAYLOAD_TYPE_ST30_AUDIO:
    //         /* audio format */
    //         param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
    //         param.payload_args.audio_args.channel = 2;
    //         param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
    //         param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
    //         param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;
    //         break;
    //     case PAYLOAD_TYPE_ST40_ANCILLARY:
    //         /* ancillary format */
    //         param.payload_args.anc_args.format = ANC_FORMAT_CLOSED_CAPTION;
    //         param.payload_args.anc_args.type = ANC_TYPE_FRAME_LEVEL;
    //         param.payload_args.anc_args.fps = s->fps;
    //         break;
    //     case PAYLOAD_TYPE_ST20_VIDEO:
    //     case PAYLOAD_TYPE_ST22_VIDEO:
    //     default:
    //         /* video format */
    //         param.payload_args.video_args.width   = param.width = s->width;
    //         param.payload_args.video_args.height  = param.height = s->height;
    //         param.payload_args.video_args.fps     = param.fps = s->fps;
    //         param.payload_args.video_args.pix_fmt = param.pix_fmt = s->pixel_format;
    //         break;
    // }

    // s->tx_handle = mcm_create_connection(&param);
    // if (!s->tx_handle) {
    //     //err(ctx, "%s, mcm_create_connection failed\n", __func__);
    //     return AVERROR(EIO);
    // }
    return 0;
}

static int mcm_write_packet(AVFormatContext* ctx, AVPacket* pkt) {

    av_log(NULL, AV_LOG_WARNING, "MCM WRITE PACKET %d [%02X %02X %02X]\n", pkt->size, pkt->data[0], pkt->data[1], pkt->data[2]);

    // McmMuxerContext* s = ctx->priv_data;
    // mcm_buffer* buf = NULL;

    // buf = mcm_dequeue_buffer(s->tx_handle, -1, NULL);
    // if (buf == NULL) {
    //     return AVERROR(EIO);
    // }

    // // if (read_test_data(pkt->data, buf, s->width, s->height, s->pixel_format) < 0) {
    // //     return AVERROR(EIO);
    // // }

    // if (mcm_enqueue_buffer(s->tx_handle, buf) != 0) {
    //     return AVERROR(EIO);
    // }

    // s->frame_count++;

    // if (s->frame_count >= s->total_frame_num) {
    //     return AVERROR(EIO);
    // }
    return 0;
}

static int mcm_write_trailer(AVFormatContext* ctx) {

    av_log(NULL, AV_LOG_WARNING, "MCM WRITE TRAILER\n");

    // McmMuxerContext* s = ctx->priv_data;

    // mcm_destroy_connection(s->tx_handle);
    return 0;
}

#define OFFSET(x) offsetof(McmMuxerContext, x)
#define ENC AV_OPT_FLAG_ENCODING_PARAM
static const AVOption mcm_tx_options[] = {
    {"ip_addr", "IP address", OFFSET(ip_addr), AV_OPT_TYPE_STRING, {.str = NULL}, .flags = ENC},
    {"port", "port", OFFSET(port), AV_OPT_TYPE_INT, {.i64 = 9001}, -1, INT_MAX, ENC},
    {"fps", "FPS", OFFSET(fps), AV_OPT_TYPE_INT, {.i64 = 30}, -1, INT_MAX, ENC},
    {"total_frame_num", "Frame number", OFFSET(total_frame_num), AV_OPT_TYPE_INT, {.i64 = 10000}, -1, INT_MAX, ENC},
    // {"payload_type", "Payload type", OFFSET(payload_type), AV_OPT_TYPE_INT, {.str = "st20"}, .flags = ENC},
    // {"protocol_type", "Protocol type", OFFSET(protocol_type), AV_OPT_TYPE_INT, {.str = "auto"}, .flags = ENC},
    {"is_master", "Is master", OFFSET(is_master), AV_OPT_TYPE_INT, {.i64 = 1}, -1, INT_MAX, ENC},
    {"interface_id", "interface ID", OFFSET(interface_id), AV_OPT_TYPE_INT, {.i64 = 0}, -1, INT_MAX, ENC},
    { NULL },
};

static const AVClass mcm_muxer_class = {
    .class_name = "mcm muxer",
    .item_name = av_default_item_name,
    .option = mcm_tx_options,
    .version = LIBAVUTIL_VERSION_INT,
    .category = AV_CLASS_CATEGORY_DEVICE_OUTPUT,
};

const FFOutputFormat ff_mcm_muxer = {
    .p.name = "mcm",
    .p.long_name = NULL_IF_CONFIG_SMALL("Media Communication Mesh output device"),
    .priv_data_size = sizeof(McmMuxerContext),
    .write_header = mcm_write_header,
    .write_packet = mcm_write_packet,
    .write_trailer = mcm_write_trailer,
    .p.video_codec = AV_CODEC_ID_RAWVIDEO, // AV_CODEC_ID_VNULL, // AV_CODEC_ID_NONE, // AV_CODEC_ID_RAWVIDEO,
    .p.flags = AVFMT_NOFILE,
    .p.priv_class = &mcm_muxer_class,
    // .p.extensions   = "mcm",
};
