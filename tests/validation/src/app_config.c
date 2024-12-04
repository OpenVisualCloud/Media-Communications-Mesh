#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "app_config.h"

void usage(FILE* fp, const char* path)
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
    fprintf(fp, "-i, --file=input_file(sender only)\t\t"
                "Input file name (optional)\n");
    fprintf(fp, "-l, --loop=is_loop(sender only)\t\t"
                "Set infinite loop sending (default: %d)\n",
        DEFAULT_INFINITE_LOOP);
    fprintf(fp, "-n, --number=frame_number(sender only)\t"
                "Total frame number to send (default: %d)\n",
        DEFAULT_TOTAL_NUM);
    fprintf(fp, "-r, --ip=ip_address(sender only)\t\t"
                "Receive data from IP address (default: %s)\n",
        DEFAULT_RECV_IP);
    fprintf(fp, "-s, --ip=ip_address(receiver only)\t\t"
                "Send data to IP address (default: %s)\n",
        DEFAULT_SEND_IP);
    fprintf(fp, "-p, --port=port_number(receiver only)\t\t"
                "Send data to Port (default: %s)\n",
        DEFAULT_SEND_PORT);
    fprintf(fp, "-k, --dumpfile=file_name(receiver only)\t"
            "Save stream to local file (example: %s)\n",
        EXAMPLE_LOCAL_FILE);
    fprintf(fp, "\n");
}

Ts_config parse_cli_input(int argc, char** argv){
    int help_flag = 0;
    int opt;
    Ts_config config ={
        .mode = RECEIVER,
        .recv_addr = DEFAULT_RECV_IP,
        .recv_port = DEFAULT_RECV_PORT,
        .send_addr = DEFAULT_SEND_IP,
        .send_port = DEFAULT_SEND_PORT,
        .protocol_type = DEFAULT_PROTOCOL,
        .payload_type = DEFAULT_PAYLOAD_TYPE,

        .file_name = "",

        .pix_fmt_string = DEFAULT_VIDEO_FMT,
        .socket_path = DEFAULT_MEMIF_SOCKET_PATH,
        .interface_id = DEFAULT_MEMIF_INTERFACE_ID,

        /* video resolution */
        .width = DEFAULT_FRAME_WIDTH,
        .height = DEFAULT_FRAME_HEIGHT,
        .vid_fps = DEFAULT_FPS
    };
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 'H' },
        { "mode", required_argument, &help_flag, 'm' },

        { "file_name", required_argument, NULL, 'b' },
        { "width", required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "fps", required_argument, NULL, 'f' },
        { "pix_fmt", required_argument, NULL, 'x' },

        { "recv_ip", required_argument, NULL, 'r' },
        { "recv_port", required_argument, NULL, 'i' },
        { "send_ip", required_argument, NULL, 's' },
        { "send_port", required_argument, NULL, 'p' },
        
        { "protocol_type", required_argument, NULL, 'o' },
        { "payload_type", required_argument, NULL, 't' },
        { "socketpath", required_argument, NULL, 'k' },
        { "interfaceid", required_argument, NULL, 'd' },
        
        { 0 }
    };
    
    while (1) {
        opt = getopt_long(argc, argv, "Hb:w:h:f:x:r:i:s:p:o:t:k:d:", longopts, 0);
        if (opt == -1)
            break;

        switch (opt) {
        case 'm':
            config.mode = atoi(optarg);
            break;
        case 'H':
            usage(stdout, argv[0]);
            exit(EXIT_FAILURE);
        case 'b':
            strcpy(config.file_name, optarg);
            break;
        case 'w':
            config.width = atoi(optarg);
            break;
        case 'h':
            config.height = atoi(optarg);
            break;
        case 'f':
            config.vid_fps = atof(optarg);
            break;
        case 'x':
            strcpy(config.pix_fmt_string, optarg);
            break;
        case 'r':
            strcpy(config.recv_addr, optarg);
            break;
        case 'i':
            strcpy(config.recv_port, optarg);
            break;
        case 's':
            strcpy(config.send_addr, optarg);
            break;
        case 'p':
            strcpy(config.send_port, optarg);
            break;
        case 'o':
            strcpy(config.protocol_type, optarg);
            break;
        case 't':
            strcpy(config.payload_type, optarg);
            break;
        case 'k':
            strcpy(config.socket_path, optarg);
            break;
        case 'd':
            config.interface_id = atoi(optarg);
            break;
        default:
            break;
        }
    }
    return config;
}