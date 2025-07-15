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
#include <time.h>
#include <sys/time.h>

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

// Multipoint group configuration
typedef struct {
    char group_name[64];         // Group identifier
    char node_name[64];          // This node's identifier
    int node_id;                 // Unique node ID (0-255)
    char payload_type[16];       // "video", "audio", "blob"
    char group_topology[16];     // "mesh", "star", "ring"
    int max_group_size;          // Maximum nodes in group
    int heartbeat_interval_ms;   // Heartbeat interval
    int sync_enabled;            // Enable frame synchronization
    double frame_rate;           // Target frame rate for sync
    char priority[16];           // "low", "normal", "high"
    char reliability[16];        // "best_effort", "reliable"
    // Media parameters
    int width;
    int height;
    double fps;
    char pixel_format[32];
    int channels;
    int sample_rate;
    char audio_format[16];
    // Test parameters
    int test_duration_sec;
    int packet_size;
    char test_pattern[32];
    char output_file[256];
} multipoint_config_t;

static multipoint_config_t mp_cfg = {
    .group_name = "test_group",
    .node_name = "tx_node",
    .node_id = 1,
    .payload_type = "video",
    .group_topology = "mesh",
    .max_group_size = 8,
    .heartbeat_interval_ms = 1000,
    .sync_enabled = 1,
    .frame_rate = 30.0,
    .priority = "normal",
    .reliability = "reliable",
    .width = 1920,
    .height = 1080,
    .fps = 30.0,
    .pixel_format = "yuv422p10le",
    .channels = 2,
    .sample_rate = 48000,
    .audio_format = "pcm_s16le",
    .test_duration_sec = 60,
    .packet_size = 8192,
    .test_pattern = "sequential",
    .output_file = ""
};

// Group statistics
typedef struct {
    uint64_t packets_sent;
    uint64_t bytes_sent;
    uint64_t packets_failed;
    uint64_t heartbeats_sent;
    uint64_t sync_events;
    struct timeval start_time;
    struct timeval last_heartbeat;
    struct timeval last_sync;
    double avg_frame_interval_ms;
    int active_group_members;
} group_stats_t;

static group_stats_t stats = {0};

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS] [input_file]\n\n");
    printf("Multipoint Group Transmitter Test Application\n\n");
    printf("Group Configuration:\n");
    printf("  -g, --group <name>         Group name (default: %s)\n", mp_cfg.group_name);
    printf("  -n, --node <name>          Node name (default: %s)\n", mp_cfg.node_name);
    printf("  --node-id <id>             Node ID 0-255 (default: %d)\n", mp_cfg.node_id);
    printf("  --topology <type>          Group topology: mesh, star, ring (default: %s)\n", mp_cfg.group_topology);
    printf("  --max-size <count>         Maximum group size (default: %d)\n", mp_cfg.max_group_size);
    printf("  --payload-type <type>      Payload type: video, audio, blob (default: %s)\n", mp_cfg.payload_type);
    printf("\n  Synchronization:\n");
    printf("  --enable-sync              Enable frame synchronization (default: %s)\n", mp_cfg.sync_enabled ? "enabled" : "disabled");
    printf("  --disable-sync             Disable frame synchronization\n");
    printf("  --frame-rate <fps>         Target frame rate for sync (default: %.1f)\n", mp_cfg.frame_rate);
    printf("  --heartbeat <ms>           Heartbeat interval in ms (default: %d)\n", mp_cfg.heartbeat_interval_ms);
    printf("\n  Quality of Service:\n");
    printf("  --priority <level>         Priority: low, normal, high (default: %s)\n", mp_cfg.priority);
    printf("  --reliability <mode>       Reliability: best_effort, reliable (default: %s)\n", mp_cfg.reliability);
    printf("\n  Media Parameters:\n");
    printf("  -W, --width <pixels>       Video width (default: %d)\n", mp_cfg.width);
    printf("  -H, --height <pixels>      Video height (default: %d)\n", mp_cfg.height);
    printf("  -f, --fps <rate>           Frame rate (default: %.1f)\n", mp_cfg.fps);
    printf("  --pixel-fmt <format>       Pixel format (default: %s)\n", mp_cfg.pixel_format);
    printf("  -c, --channels <num>       Audio channels (default: %d)\n", mp_cfg.channels);
    printf("  -r, --sample-rate <rate>   Sample rate (default: %d)\n", mp_cfg.sample_rate);
    printf("  --audio-fmt <format>       Audio format (default: %s)\n", mp_cfg.audio_format);
    printf("\n  Test Parameters:\n");
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", mp_cfg.test_duration_sec);
    printf("  -s, --packet-size <bytes>  Packet size for blob mode (default: %d)\n", mp_cfg.packet_size);
    printf("  --pattern <type>           Test pattern: sequential, random, broadcast (default: %s)\n", mp_cfg.test_pattern);
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("\n  General:\n");
    printf("  -h, --help                 Show this help\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("\n  Examples:\n");
    printf("    # Basic group transmission\n");
    printf("    %s --group broadcast_test --node sender1 input.yuv\n", prog_name);
    printf("\n    # Synchronized multi-sender setup\n");
    printf("    %s --group sync_group --enable-sync --frame-rate 60 --node cam1\n", prog_name);
    printf("\n    # High-priority audio group\n");
    printf("    %s --payload-type audio --priority high --reliability reliable\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"group", required_argument, 0, 'g'},
        {"node", required_argument, 0, 'n'},
        {"node-id", required_argument, 0, 1001},
        {"topology", required_argument, 0, 1002},
        {"max-size", required_argument, 0, 1003},
        {"payload-type", required_argument, 0, 1004},
        {"enable-sync", no_argument, 0, 1005},
        {"disable-sync", no_argument, 0, 1006},
        {"frame-rate", required_argument, 0, 1007},
        {"heartbeat", required_argument, 0, 1008},
        {"priority", required_argument, 0, 1009},
        {"reliability", required_argument, 0, 1010},
        {"width", required_argument, 0, 'W'},
        {"height", required_argument, 0, 'H'},
        {"fps", required_argument, 0, 'f'},
        {"pixel-fmt", required_argument, 0, 1011},
        {"channels", required_argument, 0, 'c'},
        {"sample-rate", required_argument, 0, 'r'},
        {"audio-fmt", required_argument, 0, 1012},
        {"test-duration", required_argument, 0, 't'},
        {"packet-size", required_argument, 0, 's'},
        {"pattern", required_argument, 0, 1013},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "g:n:W:H:f:c:r:t:s:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'g':
                strncpy(mp_cfg.group_name, optarg, sizeof(mp_cfg.group_name) - 1);
                break;
            case 'n':
                strncpy(mp_cfg.node_name, optarg, sizeof(mp_cfg.node_name) - 1);
                break;
            case 1001:
                mp_cfg.node_id = atoi(optarg);
                if (mp_cfg.node_id < 0 || mp_cfg.node_id > 255) {
                    fprintf(stderr, "Error: Node ID must be between 0 and 255\n");
                    exit(1);
                }
                break;
            case 1002:
                strncpy(mp_cfg.group_topology, optarg, sizeof(mp_cfg.group_topology) - 1);
                break;
            case 1003:
                mp_cfg.max_group_size = atoi(optarg);
                break;
            case 1004:
                strncpy(mp_cfg.payload_type, optarg, sizeof(mp_cfg.payload_type) - 1);
                break;
            case 1005:
                mp_cfg.sync_enabled = 1;
                break;
            case 1006:
                mp_cfg.sync_enabled = 0;
                break;
            case 1007:
                mp_cfg.frame_rate = atof(optarg);
                break;
            case 1008:
                mp_cfg.heartbeat_interval_ms = atoi(optarg);
                break;
            case 1009:
                strncpy(mp_cfg.priority, optarg, sizeof(mp_cfg.priority) - 1);
                break;
            case 1010:
                strncpy(mp_cfg.reliability, optarg, sizeof(mp_cfg.reliability) - 1);
                break;
            case 'W':
                mp_cfg.width = atoi(optarg);
                break;
            case 'H':
                mp_cfg.height = atoi(optarg);
                break;
            case 'f':
                mp_cfg.fps = atof(optarg);
                break;
            case 1011:
                strncpy(mp_cfg.pixel_format, optarg, sizeof(mp_cfg.pixel_format) - 1);
                break;
            case 'c':
                mp_cfg.channels = atoi(optarg);
                break;
            case 'r':
                mp_cfg.sample_rate = atoi(optarg);
                break;
            case 1012:
                strncpy(mp_cfg.audio_format, optarg, sizeof(mp_cfg.audio_format) - 1);
                break;
            case 't':
                mp_cfg.test_duration_sec = atoi(optarg);
                break;
            case 's':
                mp_cfg.packet_size = atoi(optarg);
                break;
            case 1013:
                strncpy(mp_cfg.test_pattern, optarg, sizeof(mp_cfg.test_pattern) - 1);
                break;
            case 'o':
                strncpy(mp_cfg.output_file, optarg, sizeof(mp_cfg.output_file) - 1);
                break;
            case 'v':
                // Verbose mode handled in logging
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }

    return optind;
}

char* generate_multipoint_config() {
    char *config = malloc(2048);
    if (!config) {
        LOG("[TX] Failed to allocate memory for config");
        return NULL;
    }

    if (strcmp(mp_cfg.payload_type, "video") == 0) {
        snprintf(config, 2048,
            "{\n"
            "  \"connection\": {\n"
            "    \"multipointGroup\": {\n"
            "      \"groupName\": \"%s\",\n"
            "      \"nodeName\": \"%s\",\n"
            "      \"nodeId\": %d,\n"
            "      \"topology\": \"%s\",\n"
            "      \"maxGroupSize\": %d,\n"
            "      \"heartbeatIntervalMs\": %d,\n"
            "      \"synchronization\": {\n"
            "        \"enabled\": %s,\n"
            "        \"frameRate\": %.1f\n"
            "      },\n"
            "      \"qos\": {\n"
            "        \"priority\": \"%s\",\n"
            "        \"reliability\": \"%s\"\n"
            "      }\n"
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
            mp_cfg.group_name, mp_cfg.node_name, mp_cfg.node_id,
            mp_cfg.group_topology, mp_cfg.max_group_size, mp_cfg.heartbeat_interval_ms,
            mp_cfg.sync_enabled ? "true" : "false", mp_cfg.frame_rate,
            mp_cfg.priority, mp_cfg.reliability,
            mp_cfg.width, mp_cfg.height, mp_cfg.fps, mp_cfg.pixel_format);
    } else if (strcmp(mp_cfg.payload_type, "audio") == 0) {
        snprintf(config, 2048,
            "{\n"
            "  \"connection\": {\n"
            "    \"multipointGroup\": {\n"
            "      \"groupName\": \"%s\",\n"
            "      \"nodeName\": \"%s\",\n"
            "      \"nodeId\": %d,\n"
            "      \"topology\": \"%s\",\n"
            "      \"maxGroupSize\": %d,\n"
            "      \"heartbeatIntervalMs\": %d,\n"
            "      \"synchronization\": {\n"
            "        \"enabled\": %s,\n"
            "        \"frameRate\": %.1f\n"
            "      },\n"
            "      \"qos\": {\n"
            "        \"priority\": \"%s\",\n"
            "        \"reliability\": \"%s\"\n"
            "      }\n"
            "    }\n"
            "  },\n"
            "  \"payload\": {\n"
            "    \"audio\": {\n"
            "      \"channels\": %d,\n"
            "      \"sampleRate\": %d,\n"
            "      \"format\": \"%s\"\n"
            "    }\n"
            "  }\n"
            "}",
            mp_cfg.group_name, mp_cfg.node_name, mp_cfg.node_id,
            mp_cfg.group_topology, mp_cfg.max_group_size, mp_cfg.heartbeat_interval_ms,
            mp_cfg.sync_enabled ? "true" : "false", mp_cfg.frame_rate,
            mp_cfg.priority, mp_cfg.reliability,
            mp_cfg.channels, mp_cfg.sample_rate, mp_cfg.audio_format);
    } else {
        // Blob configuration
        snprintf(config, 2048,
            "{\n"
            "  \"connection\": {\n"
            "    \"multipointGroup\": {\n"
            "      \"groupName\": \"%s\",\n"
            "      \"nodeName\": \"%s\",\n"
            "      \"nodeId\": %d,\n"
            "      \"topology\": \"%s\",\n"
            "      \"maxGroupSize\": %d,\n"
            "      \"heartbeatIntervalMs\": %d,\n"
            "      \"synchronization\": {\n"
            "        \"enabled\": %s,\n"
            "        \"frameRate\": %.1f\n"
            "      },\n"
            "      \"qos\": {\n"
            "        \"priority\": \"%s\",\n"
            "        \"reliability\": \"%s\"\n"
            "      }\n"
            "    }\n"
            "  },\n"
            "  \"payload\": {\n"
            "    \"blob\": {}\n"
            "  }\n"
            "}",
            mp_cfg.group_name, mp_cfg.node_name, mp_cfg.node_id,
            mp_cfg.group_topology, mp_cfg.max_group_size, mp_cfg.heartbeat_interval_ms,
            mp_cfg.sync_enabled ? "true" : "false", mp_cfg.frame_rate,
            mp_cfg.priority, mp_cfg.reliability);
    }

    return config;
}

void send_heartbeat() {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Check if it's time to send heartbeat
    double elapsed_ms = ((now.tv_sec - stats.last_heartbeat.tv_sec) * 1000.0) +
                       ((now.tv_usec - stats.last_heartbeat.tv_usec) / 1000.0);
                       
    if (elapsed_ms >= mp_cfg.heartbeat_interval_ms) {
        // In a real implementation, this would send a heartbeat message
        // For now, we just track the count
        stats.heartbeats_sent++;
        stats.last_heartbeat = now;
        LOG("[TX] Heartbeat #%lu sent to group '%s'", stats.heartbeats_sent, mp_cfg.group_name);
    }
}

void wait_for_sync() {
    if (!mp_cfg.sync_enabled) return;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    double target_interval_ms = 1000.0 / mp_cfg.frame_rate;
    double elapsed_ms = ((now.tv_sec - stats.last_sync.tv_sec) * 1000.0) +
                       ((now.tv_usec - stats.last_sync.tv_usec) / 1000.0);
    
    if (elapsed_ms < target_interval_ms) {
        int sleep_us = (int)((target_interval_ms - elapsed_ms) * 1000);
        if (sleep_us > 0) {
            usleep(sleep_us);
        }
    }
    
    gettimeofday(&stats.last_sync, NULL);
    stats.sync_events++;
    
    // Update average frame interval
    if (stats.sync_events > 1) {
        stats.avg_frame_interval_ms = ((stats.avg_frame_interval_ms * (stats.sync_events - 2)) + elapsed_ms) / (stats.sync_events - 1);
    }
}

void fill_group_header(uint8_t *buffer, size_t size) {
    // Add group-specific header information
    struct {
        uint32_t magic;
        uint8_t node_id;
        uint64_t sequence;
        uint64_t timestamp_us;
        uint8_t pattern_type;
    } __attribute__((packed)) header;
    
    header.magic = 0x47525550; // 'GRUP'
    header.node_id = mp_cfg.node_id;
    header.sequence = stats.packets_sent;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    header.timestamp_us = (now.tv_sec * 1000000ULL) + now.tv_usec;
    
    if (strcmp(mp_cfg.test_pattern, "sequential") == 0) {
        header.pattern_type = 1;
    } else if (strcmp(mp_cfg.test_pattern, "random") == 0) {
        header.pattern_type = 2;
    } else if (strcmp(mp_cfg.test_pattern, "broadcast") == 0) {
        header.pattern_type = 3;
    } else {
        header.pattern_type = 0;
    }
    
    if (size >= sizeof(header)) {
        memcpy(buffer, &header, sizeof(header));
        
        // Fill remainder with pattern data
        if (size > sizeof(header)) {
            size_t data_size = size - sizeof(header);
            uint8_t *data_ptr = buffer + sizeof(header);
            
            if (header.pattern_type == 1) { // sequential
                for (size_t i = 0; i < data_size; i++) {
                    data_ptr[i] = (uint8_t)((stats.packets_sent + i) & 0xFF);
                }
            } else if (header.pattern_type == 2) { // random
                srand(header.timestamp_us);
                for (size_t i = 0; i < data_size; i++) {
                    data_ptr[i] = (uint8_t)(rand() & 0xFF);
                }
            } else if (header.pattern_type == 3) { // broadcast
                memset(data_ptr, (uint8_t)(stats.packets_sent & 0xFF), data_size);
            }
        }
    }
}

void save_group_results() {
    if (strlen(mp_cfg.output_file) == 0) return;
    
    FILE *f = fopen(mp_cfg.output_file, "w");
    if (!f) {
        LOG("[TX] Failed to open output file: %s", mp_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Multipoint Group Transmission Results\n");
    fprintf(f, "Group Name: %s\n", mp_cfg.group_name);
    fprintf(f, "Node Name: %s\n", mp_cfg.node_name);
    fprintf(f, "Node ID: %d\n", mp_cfg.node_id);
    fprintf(f, "Payload Type: %s\n", mp_cfg.payload_type);
    fprintf(f, "Topology: %s\n", mp_cfg.group_topology);
    fprintf(f, "Max Group Size: %d\n", mp_cfg.max_group_size);
    fprintf(f, "Synchronization: %s\n", mp_cfg.sync_enabled ? "enabled" : "disabled");
    if (mp_cfg.sync_enabled) {
        fprintf(f, "Target Frame Rate: %.1f fps\n", mp_cfg.frame_rate);
        fprintf(f, "Average Frame Interval: %.2f ms\n", stats.avg_frame_interval_ms);
        fprintf(f, "Sync Events: %lu\n", stats.sync_events);
    }
    fprintf(f, "Priority: %s\n", mp_cfg.priority);
    fprintf(f, "Reliability: %s\n", mp_cfg.reliability);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Packets Sent: %lu\n", stats.packets_sent);
    fprintf(f, "Packets Failed: %lu\n", stats.packets_failed);
    fprintf(f, "Bytes Sent: %lu\n", stats.bytes_sent);
    fprintf(f, "Heartbeats Sent: %lu\n", stats.heartbeats_sent);
    fprintf(f, "Success Rate: %.2f%%\n", 
            (double)(stats.packets_sent * 100) / (stats.packets_sent + stats.packets_failed));
    
    double throughput_mbps = (stats.bytes_sent * 8.0) / (total_time * 1000000.0);
    fprintf(f, "Average Throughput: %.2f Mbps\n", throughput_mbps);
    
    fclose(f);
    LOG("[TX] Group test results saved to: %s", mp_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    int file_arg_index = parse_arguments(argc, argv);
    
    char *input_file = NULL;
    if (file_arg_index < argc) {
        input_file = argv[file_arg_index];
    }

    LOG("[TX] Starting Multipoint Group Transmitter");
    LOG("[TX] Group: '%s', Node: '%s' (ID: %d)", mp_cfg.group_name, mp_cfg.node_name, mp_cfg.node_id);
    LOG("[TX] Topology: %s, Max Size: %d, Payload: %s", 
        mp_cfg.group_topology, mp_cfg.max_group_size, mp_cfg.payload_type);
    LOG("[TX] Sync: %s, Priority: %s, Reliability: %s", 
        mp_cfg.sync_enabled ? "enabled" : "disabled", mp_cfg.priority, mp_cfg.reliability);

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate multipoint connection configuration
    conn_cfg = generate_multipoint_config();
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

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    stats.last_heartbeat = stats.start_time;
    stats.last_sync = stats.start_time;

    LOG("[TX] Starting group transmission for %d seconds...", mp_cfg.test_duration_sec);

    // Main transmission loop
    struct timeval test_end;
    gettimeofday(&test_end, NULL);
    test_end.tv_sec += mp_cfg.test_duration_sec;

    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        if (shutdown_flag == SHUTDOWN_REQUESTED) {
            LOG("[TX] Graceful shutdown requested");
            break;
        }
        
        // Send heartbeat if needed
        send_heartbeat();
        
        // Wait for synchronization if enabled
        wait_for_sync();
        
        // Send data based on payload type
        if (strcmp(mp_cfg.payload_type, "video") == 0 && input_file) {
            err = mcm_send_video_frames(connection, input_file, conn_cfg);
            if (err) {
                LOG("[TX] Failed to send video frame: %s (%d)", mesh_err2str(err), err);
                stats.packets_failed++;
            } else {
                stats.packets_sent++;
                stats.bytes_sent += mp_cfg.width * mp_cfg.height * 2; // Estimate for YUV422
            }
        } else if (strcmp(mp_cfg.payload_type, "audio") == 0 && input_file) {
            err = mcm_send_audio_packets(connection, input_file, conn_cfg);
            if (err) {
                LOG("[TX] Failed to send audio packet: %s (%d)", mesh_err2str(err), err);
                stats.packets_failed++;
            } else {
                stats.packets_sent++;
                stats.bytes_sent += mp_cfg.sample_rate * mp_cfg.channels * 2 / 1000; // Estimate per ms
            }
        } else {
            // Send blob data
            MeshBuffer *buf;
            err = mesh_get_buffer(connection, &buf);
            if (err) {
                LOG("[TX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
                stats.packets_failed++;
                continue;
            }
            
            size_t packet_size = (buf->payload_len < mp_cfg.packet_size) ? 
                               buf->payload_len : mp_cfg.packet_size;
            
            fill_group_header((uint8_t*)buf->payload_ptr, packet_size);
            
            err = mesh_buffer_set_payload_len(buf, packet_size);
            if (err) {
                LOG("[TX] Failed to set payload length: %s (%d)", mesh_err2str(err), err);
                mesh_put_buffer(&buf);
                stats.packets_failed++;
                continue;
            }
            
            err = mesh_put_buffer(&buf);
            if (err) {
                LOG("[TX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
                stats.packets_failed++;
            } else {
                stats.packets_sent++;
                stats.bytes_sent += packet_size;
            }
        }
        
        // Print progress periodically
        if (stats.packets_sent % 100 == 0) {
            double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                            (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
            double rate = stats.packets_sent / elapsed;
            printf("\r[TX] Progress: %.1fs | Packets: %lu | Rate: %.1f pps | Heartbeats: %lu", 
                   elapsed, stats.packets_sent, rate, stats.heartbeats_sent);
            fflush(stdout);
        }
    }
    
    printf("\n");
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[TX] Group transmission completed in %.2f seconds", total_time);
    LOG("[TX] Packets sent: %lu, Failed: %lu", stats.packets_sent, stats.packets_failed);
    LOG("[TX] Total bytes sent: %lu", stats.bytes_sent);
    LOG("[TX] Heartbeats sent: %lu", stats.heartbeats_sent);
    
    if (mp_cfg.sync_enabled) {
        LOG("[TX] Sync events: %lu, Average frame interval: %.2f ms", 
            stats.sync_events, stats.avg_frame_interval_ms);
        double actual_fps = 1000.0 / stats.avg_frame_interval_ms;
        LOG("[TX] Target FPS: %.1f, Actual FPS: %.1f", mp_cfg.frame_rate, actual_fps);
    }
    
    double throughput_mbps = (stats.bytes_sent * 8.0) / (total_time * 1000000.0);
    LOG("[TX] Average throughput: %.2f Mbps", throughput_mbps);
    
    // Save results to file if specified
    save_group_results();

safe_exit:
    LOG("[TX] Shutting down group connection");
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
