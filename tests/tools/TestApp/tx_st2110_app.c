/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

// ST 2110 configuration options
typedef struct {
    char transport[32];           // "st2110-20", "st2110-22", "st2110-30"
    char ip_addr[64];            // Destination IP
    int port;                    // Destination port
    char mcast_src_ip[64];       // Multicast source IP (optional)
    char pacing[16];             // "narrow"
    int payload_type;            // 96-127
    char transport_pixel_fmt[32]; // For st2110-20
    // Video parameters
    int width;
    int height;
    double fps;
    char pixel_format[32];
    // Audio parameters
    int channels;
    int sample_rate;
    char audio_format[16];
    char packet_time[16];
    // RDMA options
    char rdma_provider[16];
    int rdma_num_endpoints;
    // Buffer options
    int buffer_queue_capacity;
    int conn_delay_ms;
} st2110_config_t;

static st2110_config_t st2110_cfg = {
    .transport = "st2110-20",
    .ip_addr = "224.0.0.1",
    .port = 9002,
    .mcast_src_ip = "",
    .pacing = "narrow",
    .payload_type = 112,
    .transport_pixel_fmt = "yuv422p10rfc4175",
    .width = 1920,
    .height = 1080,
    .fps = 60.0,
    .pixel_format = "yuv422p10le",
    .channels = 2,
    .sample_rate = 48000,
    .audio_format = "pcm_s24be",
    .packet_time = "1ms",
    .rdma_provider = "tcp",
    .rdma_num_endpoints = 1,
    .buffer_queue_capacity = 16,
    .conn_delay_ms = 0
};

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS] <input_file>\n\n");
    printf("SMPTE ST 2110 Transmitter Test Application\n\n");
    printf("Options:\n");
    printf("  -t, --transport <type>     Transport type: st2110-20, st2110-22, st2110-30 (default: %s)\n", st2110_cfg.transport);
    printf("  -i, --ip <address>         Destination IP address (default: %s)\n", st2110_cfg.ip_addr);
    printf("  -p, --port <port>          Destination port (default: %d)\n", st2110_cfg.port);
    printf("  -s, --src-ip <address>     Multicast source IP (optional)\n");
    printf("  -P, --payload-type <type>  Payload type 96-127 (default: %d)\n", st2110_cfg.payload_type);
    printf("  --pacing <type>            Pacing type (default: %s)\n", st2110_cfg.pacing);
    printf("  --transport-fmt <fmt>      Transport pixel format for st2110-20 (default: %s)\n", st2110_cfg.transport_pixel_fmt);
    printf("\n  Video options (for st2110-20/22):\n");
    printf("  -W, --width <pixels>       Video width (default: %d)\n", st2110_cfg.width);
    printf("  -H, --height <pixels>      Video height (default: %d)\n", st2110_cfg.height);
    printf("  -f, --fps <rate>           Frame rate (default: %.1f)\n", st2110_cfg.fps);
    printf("  --pixel-fmt <format>       Pixel format (default: %s)\n", st2110_cfg.pixel_format);
    printf("\n  Audio options (for st2110-30):\n");
    printf("  -c, --channels <num>       Audio channels (default: %d)\n", st2110_cfg.channels);
    printf("  -r, --sample-rate <rate>   Sample rate (default: %d)\n", st2110_cfg.sample_rate);
    printf("  --audio-fmt <format>       Audio format (default: %s)\n", st2110_cfg.audio_format);
    printf("  --packet-time <time>       Packet time (default: %s)\n", st2110_cfg.packet_time);
    printf("\n  RDMA options:\n");
    printf("  --rdma-provider <prov>     RDMA provider: tcp, verbs (default: %s)\n", st2110_cfg.rdma_provider);
    printf("  --rdma-endpoints <num>     Number of RDMA endpoints 1-8 (default: %d)\n", st2110_cfg.rdma_num_endpoints);
    printf("\n  Buffer options:\n");
    printf("  -q, --queue-capacity <num> Buffer queue capacity (default: %d)\n", st2110_cfg.buffer_queue_capacity);
    printf("  -d, --delay <ms>           Connection creation delay in ms (default: %d)\n", st2110_cfg.conn_delay_ms);
    printf("\n  General:\n");
    printf("  -h, --help                 Show this help\n");
    printf("  -l, --loop <count>         Loop count (-1 for infinite, default: 1)\n");
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"transport", required_argument, 0, 't'},
        {"ip", required_argument, 0, 'i'},
        {"port", required_argument, 0, 'p'},
        {"src-ip", required_argument, 0, 's'},
        {"payload-type", required_argument, 0, 'P'},
        {"pacing", required_argument, 0, 1001},
        {"transport-fmt", required_argument, 0, 1002},
        {"width", required_argument, 0, 'W'},
        {"height", required_argument, 0, 'H'},
        {"fps", required_argument, 0, 'f'},
        {"pixel-fmt", required_argument, 0, 1003},
        {"channels", required_argument, 0, 'c'},
        {"sample-rate", required_argument, 0, 'r'},
        {"audio-fmt", required_argument, 0, 1004},
        {"packet-time", required_argument, 0, 1005},
        {"rdma-provider", required_argument, 0, 1006},
        {"rdma-endpoints", required_argument, 0, 1007},
        {"queue-capacity", required_argument, 0, 'q'},
        {"delay", required_argument, 0, 'd'},
        {"loop", required_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "t:i:p:s:P:W:H:f:c:r:q:d:l:h", long_options, NULL)) != -1) {
        switch (c) {
            case 't':
                strncpy(st2110_cfg.transport, optarg, sizeof(st2110_cfg.transport) - 1);
                break;
            case 'i':
                strncpy(st2110_cfg.ip_addr, optarg, sizeof(st2110_cfg.ip_addr) - 1);
                break;
            case 'p':
                st2110_cfg.port = atoi(optarg);
                break;
            case 's':
                strncpy(st2110_cfg.mcast_src_ip, optarg, sizeof(st2110_cfg.mcast_src_ip) - 1);
                break;
            case 'P':
                st2110_cfg.payload_type = atoi(optarg);
                break;
            case 1001:
                strncpy(st2110_cfg.pacing, optarg, sizeof(st2110_cfg.pacing) - 1);
                break;
            case 1002:
                strncpy(st2110_cfg.transport_pixel_fmt, optarg, sizeof(st2110_cfg.transport_pixel_fmt) - 1);
                break;
            case 'W':
                st2110_cfg.width = atoi(optarg);
                break;
            case 'H':
                st2110_cfg.height = atoi(optarg);
                break;
            case 'f':
                st2110_cfg.fps = atof(optarg);
                break;
            case 1003:
                strncpy(st2110_cfg.pixel_format, optarg, sizeof(st2110_cfg.pixel_format) - 1);
                break;
            case 'c':
                st2110_cfg.channels = atoi(optarg);
                break;
            case 'r':
                st2110_cfg.sample_rate = atoi(optarg);
                break;
            case 1004:
                strncpy(st2110_cfg.audio_format, optarg, sizeof(st2110_cfg.audio_format) - 1);
                break;
            case 1005:
                strncpy(st2110_cfg.packet_time, optarg, sizeof(st2110_cfg.packet_time) - 1);
                break;
            case 1006:
                strncpy(st2110_cfg.rdma_provider, optarg, sizeof(st2110_cfg.rdma_provider) - 1);
                break;
            case 1007:
                st2110_cfg.rdma_num_endpoints = atoi(optarg);
                break;
            case 'q':
                st2110_cfg.buffer_queue_capacity = atoi(optarg);
                break;
            case 'd':
                st2110_cfg.conn_delay_ms = atoi(optarg);
                break;
            case 'l':
                input_loop = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }

    return optind; // Return index of first non-option argument
}

char* generate_st2110_config() {
    char *config = malloc(2048);
    if (!config) {
        LOG("[TX] Failed to allocate memory for config");
        return NULL;
    }

    if (strcmp(st2110_cfg.transport, "st2110-30") == 0) {
        // Audio configuration
        snprintf(config, 2048,
            "{\n"
            "  \"bufferQueueCapacity\": %d,\n"
            "  \"connCreationDelayMilliseconds\": %d,\n"
            "  \"connection\": {\n"
            "    \"st2110\": {\n"
            "      \"transport\": \"%s\",\n"
            "      \"ipAddr\": \"%s\",\n"
            "      \"port\": %d,\n"
            "      \"multicastSourceIpAddr\": \"%s\",\n"
            "      \"pacing\": \"%s\",\n"
            "      \"payloadType\": %d\n"
            "    }\n"
            "  },\n"
            "  \"options\": {\n"
            "    \"rdma\": {\n"
            "      \"provider\": \"%s\",\n"
            "      \"numEndpoints\": %d\n"
            "    }\n"
            "  },\n"
            "  \"payload\": {\n"
            "    \"audio\": {\n"
            "      \"channels\": %d,\n"
            "      \"sampleRate\": %d,\n"
            "      \"format\": \"%s\",\n"
            "      \"packetTime\": \"%s\"\n"
            "    }\n"
            "  }\n"
            "}",
            st2110_cfg.buffer_queue_capacity, st2110_cfg.conn_delay_ms,
            st2110_cfg.transport, st2110_cfg.ip_addr, st2110_cfg.port,
            st2110_cfg.mcast_src_ip, st2110_cfg.pacing, st2110_cfg.payload_type,
            st2110_cfg.rdma_provider, st2110_cfg.rdma_num_endpoints,
            st2110_cfg.channels, st2110_cfg.sample_rate,
            st2110_cfg.audio_format, st2110_cfg.packet_time);
    } else {
        // Video configuration (st2110-20 or st2110-22)
        if (strcmp(st2110_cfg.transport, "st2110-20") == 0) {
            snprintf(config, 2048,
                "{\n"
                "  \"bufferQueueCapacity\": %d,\n"
                "  \"connCreationDelayMilliseconds\": %d,\n"
                "  \"connection\": {\n"
                "    \"st2110\": {\n"
                "      \"transport\": \"%s\",\n"
                "      \"ipAddr\": \"%s\",\n"
                "      \"port\": %d,\n"
                "      \"multicastSourceIpAddr\": \"%s\",\n"
                "      \"pacing\": \"%s\",\n"
                "      \"payloadType\": %d,\n"
                "      \"transportPixelFormat\": \"%s\"\n"
                "    }\n"
                "  },\n"
                "  \"options\": {\n"
                "    \"rdma\": {\n"
                "      \"provider\": \"%s\",\n"
                "      \"numEndpoints\": %d\n"
                "    }\n"
                "  },\n"
                "  \"payload\": {\n"
                "    \"video\": {\n"
                "      \"width\": %d,\n"
                "      \"height\": %d,\n"
                "      \"fps\": %.1f,\n"
                "      \"pixelFormat\": \"%s\"\n"
                "    }\n"
                "  }\n"
                "}",
                st2110_cfg.buffer_queue_capacity, st2110_cfg.conn_delay_ms,
                st2110_cfg.transport, st2110_cfg.ip_addr, st2110_cfg.port,
                st2110_cfg.mcast_src_ip, st2110_cfg.pacing, st2110_cfg.payload_type,
                st2110_cfg.transport_pixel_fmt,
                st2110_cfg.rdma_provider, st2110_cfg.rdma_num_endpoints,
                st2110_cfg.width, st2110_cfg.height, st2110_cfg.fps, st2110_cfg.pixel_format);
        } else {
            // st2110-22 (no transportPixelFormat)
            snprintf(config, 2048,
                "{\n"
                "  \"bufferQueueCapacity\": %d,\n"
                "  \"connCreationDelayMilliseconds\": %d,\n"
                "  \"connection\": {\n"
                "    \"st2110\": {\n"
                "      \"transport\": \"%s\",\n"
                "      \"ipAddr\": \"%s\",\n"
                "      \"port\": %d,\n"
                "      \"multicastSourceIpAddr\": \"%s\",\n"
                "      \"pacing\": \"%s\",\n"
                "      \"payloadType\": %d\n"
                "    }\n"
                "  },\n"
                "  \"options\": {\n"
                "    \"rdma\": {\n"
                "      \"provider\": \"%s\",\n"
                "      \"numEndpoints\": %d\n"
                "    }\n"
                "  },\n"
                "  \"payload\": {\n"
                "    \"video\": {\n"
                "      \"width\": %d,\n"
                "      \"height\": %d,\n"
                "      \"fps\": %.1f,\n"
                "      \"pixelFormat\": \"%s\"\n"
                "    }\n"
                "  }\n"
                "}",
                st2110_cfg.buffer_queue_capacity, st2110_cfg.conn_delay_ms,
                st2110_cfg.transport, st2110_cfg.ip_addr, st2110_cfg.port,
                st2110_cfg.mcast_src_ip, st2110_cfg.pacing, st2110_cfg.payload_type,
                st2110_cfg.rdma_provider, st2110_cfg.rdma_num_endpoints,
                st2110_cfg.width, st2110_cfg.height, st2110_cfg.fps, st2110_cfg.pixel_format);
        }
    }

    return config;
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    int file_arg_index = parse_arguments(argc, argv);
    
    if (file_arg_index >= argc) {
        fprintf(stderr, "Error: Input file required\n\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    char *input_file = argv[file_arg_index];

    LOG("[TX] Launching ST 2110 TX app with transport: %s", st2110_cfg.transport);
    LOG("[TX] Target: %s:%d, Payload Type: %d", st2110_cfg.ip_addr, st2110_cfg.port, st2110_cfg.payload_type);
    
    if (strcmp(st2110_cfg.transport, "st2110-30") == 0) {
        LOG("[TX] Audio: %d channels, %d Hz, %s, %s", 
            st2110_cfg.channels, st2110_cfg.sample_rate, 
            st2110_cfg.audio_format, st2110_cfg.packet_time);
    } else {
        LOG("[TX] Video: %dx%d @ %.1f fps, %s", 
            st2110_cfg.width, st2110_cfg.height, st2110_cfg.fps, st2110_cfg.pixel_format);
    }
    LOG("[TX] RDMA: provider=%s, endpoints=%d", st2110_cfg.rdma_provider, st2110_cfg.rdma_num_endpoints);

    // Generate default client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate ST 2110 connection configuration
    conn_cfg = generate_st2110_config();
    if (!conn_cfg) {
        free(client_cfg);
        exit(EXIT_FAILURE);
    }

    LOG("[TX] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[TX] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_tx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[TX] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Send data based on payload type */
    if (strcmp(st2110_cfg.transport, "st2110-30") == 0) {
        // Audio transmission
        if (input_loop == -1) {
            LOG("[TX] sending audio packets inf times");
            while (1) {
                err = mcm_send_audio_packets(connection, input_file, conn_cfg);
                if (err) {
                    LOG("[TX] Failed to send audio packets: %s (%d)", mesh_err2str(err), err);
                    break;
                }
                if (shutdown_flag == SHUTDOWN_REQUESTED) {
                    break;
                }
            }
        } else if (input_loop > 0) {
            LOG("[TX] sending audio packets %d times", input_loop);
            for (int i = 0; i < input_loop; i++) {
                err = mcm_send_audio_packets(connection, input_file, conn_cfg);
                if (err) {
                    LOG("[TX] Failed to send audio packets: %s (%d)", mesh_err2str(err), err);
                    break;
                }
                if (shutdown_flag == SHUTDOWN_REQUESTED) {
                    break;
                }
            }
        } else {
            LOG("[TX] sending audio packets 1 time");
            err = mcm_send_audio_packets(connection, input_file, conn_cfg);
            if (err) {
                LOG("[TX] Failed to send audio packets: %s (%d)", mesh_err2str(err), err);
            }
        }
    } else {
        // Video transmission (st2110-20 or st2110-22)
        if (input_loop == -1) {
            LOG("[TX] sending video frames inf times");
            while (1) {
                err = mcm_send_video_frames(connection, input_file, conn_cfg);
                if (err) {
                    LOG("[TX] Failed to send video frames: %s (%d)", mesh_err2str(err), err);
                    break;
                }
                if (shutdown_flag == SHUTDOWN_REQUESTED) {
                    break;
                }
            }
        } else if (input_loop > 0) {
            LOG("[TX] sending video frames %d times", input_loop);
            for (int i = 0; i < input_loop; i++) {
                err = mcm_send_video_frames(connection, input_file, conn_cfg);
                if (err) {
                    LOG("[TX] Failed to send video frames: %s (%d)", mesh_err2str(err), err);
                    break;
                }
                if (shutdown_flag == SHUTDOWN_REQUESTED) {
                    break;
                }
            }
        } else {
            LOG("[TX] sending video frames 1 time");
            err = mcm_send_video_frames(connection, input_file, conn_cfg);
            if (err) {
                LOG("[TX] Failed to send video frames: %s (%d)", mesh_err2str(err), err);
            }
        }
    }

safe_exit:
    LOG("[TX] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[TX] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    return err;
}
