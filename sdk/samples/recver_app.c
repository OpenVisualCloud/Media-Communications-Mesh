/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// TODO: Split the code, so video, audio and ancillary are separate sub-apps
// TODO: Push all code that duplicates in sender_app to sample_common

#include <bsd/string.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "sample_common.c"

static volatile bool keepRunning = true;

void intHandler(int dummy)
{
    keepRunning = 0;
}

int main(int argc, char** argv)
{
    int err = 0;
    char recv_addr[46] = DEFAULT_RECV_IP;
    char recv_port[6] = DEFAULT_RECV_PORT;
    char send_addr[46] = DEFAULT_SEND_IP;
    char send_port[6] = DEFAULT_SEND_PORT;

    char protocol_type[32] = DEFAULT_PROTOCOL;
    char payload_type[32] = DEFAULT_PAYLOAD_TYPE;
    char file_name[128] = "";
    char pix_fmt_string[32] = DEFAULT_VIDEO_FMT;
    char socket_path[108] = DEFAULT_MEMIF_SOCKET_PATH;
    uint8_t is_master = 0; // default for recver
    uint32_t interface_id = DEFAULT_MEMIF_INTERFACE_ID;

    /* video resolution */
    uint32_t width = DEFAULT_FRAME_WIDTH;
    uint32_t height = DEFAULT_FRAME_HEIGHT;
    double vid_fps = DEFAULT_FPS;
    video_pixel_format pix_fmt = PIX_FMT_YUV422P_10BIT_LE;

    char audio_type[5] = DEFAULT_AUDIO_TYPE;
    char audio_format[5] = DEFAULT_AUDIO_FORMAT;
    char audio_sampling[3] = DEFAULT_AUDIO_SAMPLING;
    char audio_ptime[6] = DEFAULT_AUDIO_PTIME;
    char anc_type[5] = DEFAULT_ANC_TYPE;
    char payload_codec[6] = DEFAULT_PAYLOAD_CODEC;
    uint32_t audio_channels = DEFAULT_AUDIO_CHANNELS;

    mcm_conn_context* dp_ctx = NULL;
    mcm_conn_param param = {};
    mcm_buffer* buf = NULL;

    uint32_t frm_size = 0;

    int help_flag = 0;
    int opt;
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 'H' },
        { "width", required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "fps", required_argument, NULL, 'f' },
        { "rcv_ip", required_argument, NULL, 'r' },
        { "rcv_port", required_argument, NULL, 'i' },
        { "send_ip", required_argument, NULL, 's' },
        { "send_port", required_argument, NULL, 'p' },
        { "protocol", required_argument, NULL, 'o' },
        { "type", required_argument, NULL, 't' },
        { "socketpath", required_argument, NULL, 'k' },
        { "master", required_argument, NULL, 'm' },
        { "interfaceid", required_argument, NULL, 'd' },
        { "dumpfile", required_argument, NULL, 'b' },
        { "pix_fmt", required_argument, NULL, 'x' },
        { "audio_type", required_argument, NULL, 'a' },
        { "audio_format", required_argument, NULL, 'j' },
        { "audio_sampling", required_argument, NULL, 'g' },
        { "audio_ptime", required_argument, NULL, 'e' },
        { "audio_channels", required_argument, NULL, 'c' },
        { "anc_type", required_argument, NULL, 'q' },
        { 0 }
    };

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv,
                          "Hw:h:f:r:i:s:p:o:t:k:m:d:b:x:a:j:g:e:c:q:",
                          longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'H':
            help_flag = 1;
            break;
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case 'f':
            vid_fps = atof(optarg);
            break;
        case 'r':
            strlcpy(recv_addr, optarg, sizeof(recv_addr));
            break;
        case 'i':
            strlcpy(recv_port, optarg, sizeof(recv_port));
            break;
        case 's':
            strlcpy(send_addr, optarg, sizeof(send_addr));
            break;
        case 'p':
            strlcpy(send_port, optarg, sizeof(send_port));
            break;
        case 'o':
            strlcpy(protocol_type, optarg, sizeof(protocol_type));
            break;
        case 't':
            strlcpy(payload_type, optarg, sizeof(payload_type));
            break;
        case 'b':
            strlcpy(file_name, optarg, sizeof(file_name));
            break;
        case 'k':
            strlcpy(socket_path, optarg, sizeof(socket_path));
            break;
        case 'm':
            is_master = atoi(optarg);
            break;
        case 'd':
            interface_id = atoi(optarg);
            break;
        case 'x':
            strlcpy(pix_fmt_string, optarg, sizeof(pix_fmt_string));
            if (strncmp(pix_fmt_string, "yuv422p", sizeof(pix_fmt_string)) == 0){
                pix_fmt = PIX_FMT_YUV422P;
            } else if (strncmp(pix_fmt_string, "yuv422p10le", sizeof(pix_fmt_string)) == 0) {
                pix_fmt = PIX_FMT_YUV422P_10BIT_LE;
            } else if (strncmp(pix_fmt_string, "yuv444p10le", sizeof(pix_fmt_string)) == 0){
                pix_fmt = PIX_FMT_YUV444P_10BIT_LE;
            } else if (strncmp(pix_fmt_string, "rgb8", sizeof(pix_fmt_string)) == 0){
                pix_fmt = PIX_FMT_RGB8;
            } else {
                pix_fmt = PIX_FMT_NV12;
            }
            break;
        case 'a':
            strlcpy(audio_type, optarg, sizeof(audio_type));
            break;
        case 'j':
            strlcpy(audio_format, optarg, sizeof(audio_format));
            break;
        case 'g':
            strlcpy(audio_sampling, optarg, sizeof(audio_sampling));
            break;
        case 'e':
            strlcpy(audio_ptime, optarg, sizeof(audio_ptime));
            break;
        case 'c':
            audio_channels = atoi(optarg);
            break;
        case 'q':
            strlcpy(anc_type, optarg, sizeof(anc_type));
            break;
        case '?':
            usage(stderr, argv[0], 0);
            return 1;
        default:
            break;
        }
    }

    if (help_flag) {
        usage(stdout, argv[0], 0);
        return 0;
    }

    /* is receiver */
    param.type = is_rx;

    /* protocol type */
    if (strncmp(protocol_type, "memif", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_MEMIF;
        strlcpy(param.memif_interface.socket_path, socket_path, sizeof(param.memif_interface.socket_path));
        param.memif_interface.is_master = is_master;
        param.memif_interface.interface_id = interface_id;
    } else if (strncmp(protocol_type, "udp", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_UDP;
    } else if (strncmp(protocol_type, "tcp", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_TCP;
    } else if (strncmp(protocol_type, "http", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_HTTP;
    } else if (strncmp(protocol_type, "grpc", sizeof(protocol_type)) == 0) {
        param.protocol = PROTO_GRPC;
    } else {
        param.protocol = PROTO_AUTO;
    }

    /* payload type */
    if (strncmp(payload_type, "st20", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_ST20_VIDEO;
    } else if (strncmp(payload_type, "st22", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_ST22_VIDEO;
    } else if (strncmp(payload_type, "st30", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_ST30_AUDIO;
    } else if (strncmp(payload_type, "st40", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_ST40_ANCILLARY;
    } else if (strncmp(payload_type, "rtsp", sizeof(payload_type)) == 0) {
        param.payload_type = PAYLOAD_TYPE_RTSP_VIDEO;
    } else {
        param.payload_type = PAYLOAD_TYPE_NONE;
    }

    // TODO: Move whole switch-case to common
    switch (param.payload_type) {
    case PAYLOAD_TYPE_ST30_AUDIO:
        // mcm_audio_type
        if (strncmp(audio_type, "frame", sizeof(audio_type)) == 0) {
            param.payload_args.audio_args.type = AUDIO_TYPE_FRAME_LEVEL;
        } else if (strncmp(audio_type, "rtp", sizeof(audio_type)) == 0) {
            param.payload_args.audio_args.type = AUDIO_TYPE_RTP_LEVEL;
        }
        /* TODO: Only 1 to 8 channels are supported here now
                 Tested only with 1 and 2 channels*/
        if (audio_channels > 0 && audio_channels < 9){
            param.payload_args.audio_args.channel = audio_channels;
        }
        // mcm_audio_format
        if (strncmp(audio_format, "pcm8", sizeof(audio_format)) == 0) {
            param.payload_args.audio_args.format = AUDIO_FMT_PCM8;
        } else if (strncmp(audio_format, "pcm16", sizeof(audio_format)) == 0) {
            param.payload_args.audio_args.format = AUDIO_FMT_PCM16;
        } else if (strncmp(audio_format, "pcm24", sizeof(audio_format)) == 0) {
            param.payload_args.audio_args.format = AUDIO_FMT_PCM24;
        } else if (strncmp(audio_format, "am824", sizeof(audio_format)) == 0) {
            param.payload_args.audio_args.format = AUDIO_FMT_AM824;
        }
        // mcm_audio_sampling
        if (strncmp(audio_sampling, "48k", sizeof(audio_sampling)) == 0) {
            param.payload_args.audio_args.sampling = AUDIO_SAMPLING_48K;
        } else if (strncmp(audio_sampling, "96k", sizeof(audio_sampling)) == 0) {
            param.payload_args.audio_args.sampling = AUDIO_SAMPLING_96K;
        } else if (strncmp(audio_sampling, "44k", sizeof(audio_sampling)) == 0) {
            param.payload_args.audio_args.sampling = AUDIO_SAMPLING_44K;
        }
        // mcm_audio_ptime
        if (strncmp(audio_ptime, "1ms", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_1MS;
        } else if (strncmp(audio_ptime, "125us", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_125US;
        } else if (strncmp(audio_ptime, "250us", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_250US;
        } else if (strncmp(audio_ptime, "333us", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_333US;
        } else if (strncmp(audio_ptime, "4ms", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_4MS;
        } else if (strncmp(audio_ptime, "80us", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_80US;
        } else if (strncmp(audio_ptime, "1.09ms", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_1_09MS;
        } else if (strncmp(audio_ptime, "0.14ms", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_0_14MS;
        } else if (strncmp(audio_ptime, "0.09ms", sizeof(audio_ptime)) == 0) {
            param.payload_args.audio_args.ptime = AUDIO_PTIME_0_09MS;
        }
        frm_size = getAudioFrameSize(
                        param.payload_args.audio_args.format,
                        param.payload_args.audio_args.sampling,
                        param.payload_args.audio_args.ptime,
                        param.payload_args.audio_args.channel
        );
        break;
    case PAYLOAD_TYPE_ST40_ANCILLARY:
        // mcm_anc_format
        param.payload_args.anc_args.format = ANC_FORMAT_CLOSED_CAPTION; // the only possible value
        // mcm_anc_type
        if (strncmp(anc_type, "frame", sizeof(anc_type)) == 0) {
            param.payload_args.audio_args.type = ANC_TYPE_FRAME_LEVEL;
        } else if (strncmp(anc_type, "rtp", sizeof(anc_type)) == 0) {
            param.payload_args.audio_args.type = ANC_TYPE_RTP_LEVEL;
        }
        param.payload_args.anc_args.fps = vid_fps;
        break;
    case PAYLOAD_TYPE_ST22_VIDEO:
        if (strncmp(payload_codec, "jpegxs", sizeof(payload_codec)) == 0) {
            param.payload_codec = PAYLOAD_CODEC_JPEGXS;
        } else if (strncmp(payload_codec, "h264", sizeof(payload_codec)) == 0) {
            param.payload_codec = PAYLOAD_CODEC_H264;
        }
    case PAYLOAD_TYPE_RTSP_VIDEO:
    case PAYLOAD_TYPE_ST20_VIDEO:
    default:
        /* video format */
        param.payload_args.video_args.width   = param.width = width;
        param.payload_args.video_args.height  = param.height = height;
        param.payload_args.video_args.fps     = param.fps = vid_fps;
        param.payload_args.video_args.pix_fmt = param.pix_fmt = pix_fmt;
        frm_size = getFrameSize(pix_fmt, width, height, false);
        break;
    }

    strlcpy(param.remote_addr.ip, recv_addr, sizeof(param.remote_addr.ip));
    strlcpy(param.local_addr.port, recv_port, sizeof(param.local_addr.port));
    strlcpy(param.remote_addr.port, send_port, sizeof(param.remote_addr.port));
    strlcpy(param.local_addr.ip, send_addr, sizeof(param.local_addr.ip));
    printf("LOCAL: %s:%s\n", param.local_addr.ip, param.local_addr.port);
    printf("REMOTE: %s:%s\n", param.remote_addr.ip, param.remote_addr.port);

    dp_ctx = mcm_create_connection(&param);
    if (dp_ctx == NULL) {
        printf("Fail to connect to MCM data plane\n");
        exit(-1);
    }
    signal(SIGINT, intHandler);

    uint32_t frame_count = 0;

    const uint32_t fps_interval = 30;
    double fps = 0.0;
    void *ptr = NULL;
    int timeout = -1;
    bool first_frame = true;
    float latency = 0;
    struct timespec ts_recv = {}, ts_send = {};
    struct timespec ts_begin = {}, ts_end = {};
    FILE* dump_fp = NULL;

    if (strlen(file_name) > 0) {
        dump_fp = fopen(file_name, "wb");
    }

    while (keepRunning) {
        /* receive frame */
        if (first_frame) {
            /* infinity for the 1st frame. */
            timeout = -1;
        } else {
            /* 1 second */
            timeout = 1000;
        }

        buf = mcm_dequeue_buffer(dp_ctx, timeout, &err);
        if (buf == NULL) {
            if (err == 0) {
                printf("Read buffer timeout\n");
            } else {
                printf("Failed to read buffer\n");
            }
            break;
        }
        printf("INFO: buf->len = %ld frame size = %u\n", buf->len, frm_size);

        clock_gettime(CLOCK_REALTIME, &ts_recv);
        if (first_frame) {
            ts_begin = ts_recv;
            first_frame = false;
        }

        if (param.payload_type != PAYLOAD_TYPE_RTSP_VIDEO) {
            if (dump_fp) {
                fwrite(buf->data, buf->len, 1, dump_fp);
            } else {
                // Following code are mainly for test purpose, it requires the sender side to
                // pre-set the first several bytes
                ptr = buf->data;
                if (*(uint32_t *)ptr != frame_count) {
                    printf("Wrong data content: expected %u, got %u\n", frame_count, *(uint32_t*)ptr);
                    /* catch up the sender frame count */
                    frame_count = *(uint32_t*)ptr;
                }
                ptr += sizeof(frame_count);
                ts_send = *(struct timespec *)ptr;

                if (frame_count % fps_interval == 0) {
                    /* calculate FPS */
                    clock_gettime(CLOCK_REALTIME, &ts_end);

                    fps = 1e9 * (ts_end.tv_sec - ts_begin.tv_sec);
                    fps += (ts_end.tv_nsec - ts_begin.tv_nsec);
                    fps /= 1e9;
                    fps = (double)fps_interval / fps;

                    clock_gettime(CLOCK_REALTIME, &ts_begin);
                }

                latency = 1000.0 * (ts_recv.tv_sec - ts_send.tv_sec);
                latency += (ts_recv.tv_nsec - ts_send.tv_nsec) / 1000000.0;

                printf("RX frames: [%u], latency: %0.1f ms, FPS: %0.3f\n", frame_count, latency, fps);
            }
        } else { //TODO: rtsp receiver side test code
            if (dump_fp) {
                fwrite(buf->data, buf->len, 1, dump_fp);
            }
            printf("RX package number:%d   seq_num: %d, timestamp: %u, RX H264 NALU: %ld\n", frame_count, buf->metadata.seq_num, buf->metadata.timestamp, buf->len);
        }

        frame_count++;

        /* enqueue buffer */
        if (mcm_enqueue_buffer(dp_ctx, buf) != 0) {
            break;
        }
    }

    /* Clean up */
    if (dump_fp) {
        fclose(dump_fp);
    }
    printf("Destroy MCM connection\n");
    mcm_destroy_connection(dp_ctx);

    return 0;
}
