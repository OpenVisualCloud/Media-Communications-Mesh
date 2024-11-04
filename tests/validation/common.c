#include "mcm_dp.h"
#include "../../sdk/include/mesh_dp.h"
#include <stdlib.h>
#include <stdio.h>
#include <bsd/string.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#define DEFAULT_FRAME_WIDTH 1920
#define DEFAULT_FRAME_HEIGHT 1080
#define DEFAULT_FPS 30.0
#define DEFAULT_RECV_IP "192.168.96.1"
#define DEFAULT_RECV_PORT "9001"
#define DEFAULT_SEND_IP "192.168.96.2"
#define DEFAULT_SEND_PORT "9002"
#define DEFAULT_PROTOCOL "auto"
#define DEFAULT_PAYLOAD_TYPE "st20"
#define DEFAULT_TOTAL_NUM 0 // sender only
#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"
#define DEFAULT_MEMIF_INTERFACE_ID 0
#define DEFAULT_INFINITY_LOOP 0 // sender only
#define DEFAULT_LOCAL_FILE "data-sdk.264" // recver only
#define DEFAULT_PIX_FMT_STRING "yuv422p10le"
#define DEFAULT_PIX_FMT PIX_FMT_YUV422P_10BIT_LE
#define DEFAULT_AUDIO_TYPE "frame"
#define DEFAULT_AUDIO_FORMAT "pcm16"
#define DEFAULT_AUDIO_SAMPLING "48k"
#define DEFAULT_AUDIO_PTIME "1ms"
#define DEFAULT_AUDIO_CHANNELS 1
#define DEFAULT_ANC_TYPE "frame"
#define DEFAULT_PAYLOAD_CODEC "jpegxs"
#define DEFAULT_MESH_CONN_TRANSPORT 0 // ST2110-20 (raw)


void set_video_pix_fmt(video_pixel_format* pix_fmt, char* pix_fmt_string){
    if (strncmp(pix_fmt_string, "yuv422p", sizeof(*pix_fmt_string)) == 0){
        *pix_fmt = PIX_FMT_YUV422P;
    } else if (strncmp(pix_fmt_string, "yuv422p10le", sizeof(*pix_fmt_string)) == 0) {
        *pix_fmt = PIX_FMT_YUV422P_10BIT_LE;
    } else if (strncmp(pix_fmt_string, "yuv444p10le", sizeof(*pix_fmt_string)) == 0){
        *pix_fmt = PIX_FMT_YUV444P_10BIT_LE;
    } else if (strncmp(pix_fmt_string, "rgb8", sizeof(*pix_fmt_string)) == 0){
        *pix_fmt = PIX_FMT_RGB8;
    } else {
        *pix_fmt = PIX_FMT_NV12;
    }
}

void set_video_payload_type(int* payload_type, char* payload_type_string){
    /* payload type */
    if (strncmp(payload_type_string, "st20", sizeof(*payload_type_string)) == 0) {
        *payload_type = MESH_CONN_TRANSPORT_ST2110_20;
    } else if (strncmp(payload_type_string, "st22", sizeof(*payload_type_string)) == 0) {
        *payload_type = MESH_CONN_TRANSPORT_ST2110_22;
    } else {
        *payload_type = PAYLOAD_TYPE_NONE; //TODO: Fixme?
    }
}

// char* open_file(int* len)
// {
//     // open file here
//     FILE* fptr = fopen("1080p_yuv422_10b_1.yuv", "rb");
//     if (fptr == NULL)
//     {
//         printf("Unable to open file");
//         exit(1);
//     }
// }

// void get_user_video_frames(char* data, size_t* data_len)
// {
//     // ...
// }

// void put_user_video_frames(char* data, size_t* data_len)
// {
//     // ...
// }

static void usage()
{
    // /* take only the last portion of the path */
    // const char* basename = strrchr(path, '/');
    // basename = basename ? basename + 1 : path;

    // fprintf(fp, "Usage: %s [OPTION]\n", basename);
    printf("Usage ...");
}