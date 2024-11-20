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
