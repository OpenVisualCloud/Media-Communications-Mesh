#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include "mesh_dp.h"
#include "mcm_dp.h"

// #include <assert.h>
// #include <string.h>
// #include <getopt.h>
// #include <linux/limits.h>
// #include <signal.h>
// #include <stdbool.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/stat.h>
// #include <time.h>
// #include <unistd.h>


#define DEFAULT_RECV_IP "127.0.0.1"
#define DEFAULT_RECV_PORT "9001"
#define DEFAULT_SEND_IP "127.0.0.1"
#define DEFAULT_SEND_PORT "9001"
#define DEFAULT_FRAME_WIDTH 1920
#define DEFAULT_FRAME_HEIGHT 1080
#define DEFAULT_FPS 30.0
#define DEFAULT_PAYLOAD_TYPE "st20"
#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"
#define DEFAULT_MEMIF_INTERFACE_ID 0
#define DEFAULT_PROTOCOL "auto"
#define DEFAULT_VIDEO_FMT "yuv422p10le"
#define DEFAULT_TOTAL_NUM 300
#define DEFAULT_INFINITE_LOOP 0
#define EXAMPLE_LOCAL_FILE "sample_video.yuv"
#define PORT_LENGHT 5


typedef enum Em_app_type{
    SENDER=0,
    RECEIVER
};

typedef struct Ts_config {
    int mode;
    char recv_addr[MESH_IP_ADDRESS_SIZE];
    char recv_port[5];
    char send_addr[MESH_IP_ADDRESS_SIZE];
    char send_port[5];
    char protocol_type[32];
    char payload_type[32];

    char file_name[128];

    char pix_fmt_string[32];
    char socket_path[108];
    uint32_t interface_id;

    /* video resolution */
    uint32_t width;
    uint32_t height;
    double vid_fps;

}Ts_config;
Ts_config parse_cli_input(int argc, char** argv);

#endif
