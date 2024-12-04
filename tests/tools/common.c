/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

/* print a description of all supported options */
void usage(FILE* fp, const char* path, int sender)
{
    /* take only the last portion of the path */
    const char* basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(fp, "Usage: %s [OPTION]\n", basename);
    fprintf(fp, "-w, --width=<frame_width>\t"
                "Width of test video frame (default: %d)\n",
        DEFAULT_FRAME_WIDTH);
    fprintf(fp, "-h, --height=<frame_height>\t"
                "Height of test video frame (default: %d)\n",
        DEFAULT_FRAME_HEIGHT);
    fprintf(fp, "-f, --fps=<video_fps>\t\t"
                "Test video FPS (frame per second) (default: %0.2f)\n",
        DEFAULT_FPS);
    fprintf(fp, "-o, --protocol=protocol_type\t"
                "Set protocol type (default: %s)\n",
        DEFAULT_PROTOCOL);
    fprintf(fp, "-s, --socketpath=socket_path\t"
                "Set memif socket path (default: %s)\n",
        DEFAULT_MEMIF_SOCKET_PATH);
    fprintf(fp, "-d, --interfaceid=interface_id\t"
                "Set memif conn interface id (default: %d)\n",
        DEFAULT_MEMIF_INTERFACE_ID);
    fprintf(fp, "-x, --pix_fmt=mcm_pix_fmt\t"
                "Set pix_fmt conn color format (default: %s)\n",
        DEFAULT_VIDEO_FMT);
    fprintf(fp, "-t, --type=payload_type\t\t"
                "Payload type (default: %s)\n",
        DEFAULT_PAYLOAD_TYPE);
    fprintf(fp, "-p, --port=port_number\t\t"
                "Receive data from Port (default: %s)\n",
        DEFAULT_RECV_PORT);
    fprintf(fp, "-ac, --audio_channels=<audio_channel>\t"
                "Set audio channels (default: %i)\n",
        DEFAULT_AUDIO_CHANNELS);
    fprintf(fp, "-as, --audio_sample=<audio_sample>\t"
                "Set audio sample rate (default: %0.2f)\n",
        DEFAULT_AUDIO_SAMPLE_RATE);
    fprintf(fp, "-af, --audio_format=<audio_format>\t"
                "Set audio format (default: %s)\n",
        DEFAULT_AUDIO_FORMAT);
    fprintf(fp, "-ap, --audio_ptime=<audio_ptime>\t"
                "Set audio packet time (default: %0.2f)\n",
        DEFAULT_AUDIO_PACKET_TIME);

    if (sender) {
        fprintf(fp, "-i, --file=input_file\t\t"
                    "Input file name (optional)\n");
        fprintf(fp, "-l, --loop=is_loop\t\t"
                    "Set infinite loop sending (default: %d)\n",
            DEFAULT_INFINITE_LOOP);
        fprintf(fp, "-n, --number=frame_number\t"
                    "Total frame number to send (default: %d)\n",
            DEFAULT_TOTAL_NUM);
        fprintf(fp, "-r, --ip=ip_address\t\t"
                    "Receive data from IP address (default: %s)\n",
            DEFAULT_RECV_IP);
    } else { /* receiver */
        fprintf(fp, "-s, --ip=ip_address\t\t"
                    "Send data to IP address (default: %s)\n",
            DEFAULT_SEND_IP);
        fprintf(fp, "-p, --port=port_number\t\t"
                    "Send data to Port (default: %s)\n",
            DEFAULT_SEND_PORT);
        fprintf(fp, "-k, --dumpfile=file_name\t"
                "Save stream to local file (example: %s)\n",
            EXAMPLE_LOCAL_FILE);
    }

    fprintf(fp, "\n");
}

void set_video_pix_fmt(int* pix_fmt, char* pix_fmt_string) {
    if (!strcmp(pix_fmt_string, "yuv444p10le")) {
        *pix_fmt = PIX_FMT_YUV444P_10BIT_LE; /* 3 */
    } else if (!strcmp(pix_fmt_string, "yuv422p10le")) {
        *pix_fmt = PIX_FMT_YUV422P_10BIT_LE; /* 2 */
    } else if (!strcmp(pix_fmt_string, "yuv422p")) {
        *pix_fmt = PIX_FMT_YUV422P; /* 1 */
    } else if (!strcmp(pix_fmt_string, "rgb8")) {
        *pix_fmt = PIX_FMT_RGB8; /* 4 */
    } else {
        *pix_fmt = PIX_FMT_NV12; /* 0 */
    }
}

void set_audio_fmt(int* audio_fmt, char* audio_fmt_string) {
    if (!strcmp(audio_fmt_string, "pcm8")) {
        *audio_fmt = AUDIO_FMT_PCM8; /* 0 */
    } else if (!strcmp(audio_fmt_string, "pcm16")) {
        *audio_fmt = AUDIO_FMT_PCM16; /* 1 */
    } else if (!strcmp(audio_fmt_string, "pcm24")) {
        *audio_fmt = AUDIO_FMT_PCM24; /* 2 */
    } else if (!strcmp(audio_fmt_string, "am824")) {
        *audio_fmt = AUDIO_FMT_AM824; /* 3 */
    } else {
        *audio_fmt = AUDIO_FMT_MAX; /* 4 */
    }
}

void set_audio_sampling(int* audio_smpl, double audio_smpl_double) {
    const double epsilon = 0.0001;
    if (fabs(audio_smpl_double - 48.0) < epsilon) {
        *audio_smpl = AUDIO_SAMPLING_48K; /* 0 */
    } else if (fabs(audio_smpl_double - 96.0) < epsilon) {
        *audio_smpl = AUDIO_SAMPLING_96K; /* 1 */
    } else if (fabs(audio_smpl_double - 44.1) < epsilon) {
        *audio_smpl = AUDIO_SAMPLING_44K; /* 2 */
    } else {
        *audio_smpl = AUDIO_SAMPLING_MAX; /* 3 */
    }
}

void set_audio_ptime(int* audio_ptime, double audio_ptime_double) {
    const double epsilon = 0.0001;
    if (fabs(audio_ptime_double - 1.0) < epsilon) {
        *audio_ptime = AUDIO_PTIME_1MS; /* 0 */
    } else if (fabs(audio_ptime_double - 0.125) < epsilon) {
        *audio_ptime = AUDIO_PTIME_125US; /* 1 */
    } else if (fabs(audio_ptime_double - 0.250) < epsilon) {
        *audio_ptime = AUDIO_PTIME_250US; /* 2 */
    } else if (fabs(audio_ptime_double - 0.333) < epsilon) {
        *audio_ptime = AUDIO_PTIME_333US; /* 3 */
    } else if (fabs(audio_ptime_double - 4.0) < epsilon) {
        *audio_ptime = AUDIO_PTIME_4MS; /* 4 */
    } else if (fabs(audio_ptime_double - 0.08) < epsilon) {
        *audio_ptime = AUDIO_PTIME_80US; /* 5 */
    } else if (fabs(audio_ptime_double - 1.09) < epsilon) {
        *audio_ptime = AUDIO_PTIME_1_09MS; /* 6 */
    } else if (fabs(audio_ptime_double - 0.14) < epsilon) {
        *audio_ptime = AUDIO_PTIME_0_14MS; /* 7 */
    } else if (fabs(audio_ptime_double - 0.09) < epsilon) {
        *audio_ptime = AUDIO_PTIME_0_09MS; /* 8 */
    } else {
        *audio_ptime = AUDIO_PTIME_MAX; /* 9 */
    }
}