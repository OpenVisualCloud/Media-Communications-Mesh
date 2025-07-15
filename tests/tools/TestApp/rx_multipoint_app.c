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
#include <math.h>

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
    int heartbeat_timeout_ms;    // Heartbeat timeout
    int sync_enabled;            // Enable frame synchronization
    double frame_rate;           // Expected frame rate for sync
    char priority[16];           // "low", "normal", "high"
    char reliability[16];        // "best_effort", "reliable"
    // Test parameters
    int test_duration_sec;
    int timeout_ms;              // Receive timeout
    int enable_member_tracking;  // Track group members
    int enable_sync_analysis;    // Analyze synchronization
    int enable_pattern_verify;   // Verify data patterns
    char output_file[256];
    char dump_file[256];
} multipoint_config_t;

static multipoint_config_t mp_cfg = {
    .group_name = "test_group",
    .node_name = "rx_node",
    .node_id = 2,
    .payload_type = "video",
    .group_topology = "mesh",
    .max_group_size = 8,
    .heartbeat_timeout_ms = 3000,
    .sync_enabled = 1,
    .frame_rate = 30.0,
    .priority = "normal",
    .reliability = "reliable",
    .test_duration_sec = 60,
    .timeout_ms = 1000,
    .enable_member_tracking = 1,
    .enable_sync_analysis = 1,
    .enable_pattern_verify = 0,
    .output_file = "",
    .dump_file = ""
};

// Group member tracking
typedef struct {
    uint8_t node_id;
    char node_name[64];
    uint64_t packets_received;
    uint64_t last_sequence;
    uint64_t packets_lost;
    struct timeval last_seen;
    struct timeval last_heartbeat;
    double avg_interval_ms;
    int is_active;
} group_member_t;

static group_member_t members[256]; // Index by node_id
static int member_count = 0;

// Group statistics
typedef struct {
    uint64_t total_packets_received;
    uint64_t total_bytes_received;
    uint64_t heartbeats_received;
    uint64_t sync_violations;
    uint64_t pattern_errors;
    uint64_t timeout_events;
    struct timeval start_time;
    struct timeval last_packet_time;
    double avg_frame_interval_ms;
    double sync_drift_ms;
    int active_members;
    int max_members_seen;
} group_stats_t;

static group_stats_t stats = {0};

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Multipoint Group Receiver Test Application\n\n");
    printf("Group Configuration:\n");
    printf("  -g, --group <name>         Group name (default: %s)\n", mp_cfg.group_name);
    printf("  -n, --node <name>          Node name (default: %s)\n", mp_cfg.node_name);
    printf("  --node-id <id>             Node ID 0-255 (default: %d)\n", mp_cfg.node_id);
    printf("  --topology <type>          Group topology: mesh, star, ring (default: %s)\n", mp_cfg.group_topology);
    printf("  --max-size <count>         Maximum group size (default: %d)\n", mp_cfg.max_group_size);
    printf("  --payload-type <type>      Payload type: video, audio, blob (default: %s)\n", mp_cfg.payload_type);
    printf("\n  Synchronization:\n");
    printf("  --enable-sync              Enable sync analysis (default: %s)\n", mp_cfg.sync_enabled ? "enabled" : "disabled");
    printf("  --disable-sync             Disable sync analysis\n");
    printf("  --frame-rate <fps>         Expected frame rate (default: %.1f)\n", mp_cfg.frame_rate);
    printf("  --heartbeat-timeout <ms>   Heartbeat timeout in ms (default: %d)\n", mp_cfg.heartbeat_timeout_ms);
    printf("\n  Quality of Service:\n");
    printf("  --priority <level>         Priority: low, normal, high (default: %s)\n", mp_cfg.priority);
    printf("  --reliability <mode>       Reliability: best_effort, reliable (default: %s)\n", mp_cfg.reliability);
    printf("\n  Test Parameters:\n");
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", mp_cfg.test_duration_sec);
    printf("  --timeout <ms>             Receive timeout in ms (default: %d)\n", mp_cfg.timeout_ms);
    printf("\n  Analysis Options:\n");
    printf("  --enable-tracking          Enable group member tracking (default: %s)\n", mp_cfg.enable_member_tracking ? "enabled" : "disabled");
    printf("  --disable-tracking         Disable group member tracking\n");
    printf("  --enable-verify            Enable pattern verification\n");
    printf("  --disable-verify           Disable pattern verification (default)\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  --dump <file>              Dump received data to file\n");
    printf("\n  General:\n");
    printf("  -h, --help                 Show this help\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("\n  Examples:\n");
    printf("    # Basic group receiver\n");
    printf("    %s --group broadcast_test --node receiver1\n", prog_name);
    printf("\n    # Synchronized multi-receiver with analysis\n");
    printf("    %s --group sync_group --enable-sync --enable-tracking\n", prog_name);
    printf("\n    # Data integrity monitoring\n");
    printf("    %s --enable-verify --enable-tracking --dump group_data.bin\n", prog_name);
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
        {"heartbeat-timeout", required_argument, 0, 1008},
        {"priority", required_argument, 0, 1009},
        {"reliability", required_argument, 0, 1010},
        {"test-duration", required_argument, 0, 't'},
        {"timeout", required_argument, 0, 1011},
        {"enable-tracking", no_argument, 0, 1012},
        {"disable-tracking", no_argument, 0, 1013},
        {"enable-verify", no_argument, 0, 1014},
        {"disable-verify", no_argument, 0, 1015},
        {"output", required_argument, 0, 'o'},
        {"dump", required_argument, 0, 1016},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "g:n:t:o:vh", long_options, NULL)) != -1) {
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
                mp_cfg.heartbeat_timeout_ms = atoi(optarg);
                break;
            case 1009:
                strncpy(mp_cfg.priority, optarg, sizeof(mp_cfg.priority) - 1);
                break;
            case 1010:
                strncpy(mp_cfg.reliability, optarg, sizeof(mp_cfg.reliability) - 1);
                break;
            case 't':
                mp_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1011:
                mp_cfg.timeout_ms = atoi(optarg);
                break;
            case 1012:
                mp_cfg.enable_member_tracking = 1;
                break;
            case 1013:
                mp_cfg.enable_member_tracking = 0;
                break;
            case 1014:
                mp_cfg.enable_pattern_verify = 1;
                break;
            case 1015:
                mp_cfg.enable_pattern_verify = 0;
                break;
            case 'o':
                strncpy(mp_cfg.output_file, optarg, sizeof(mp_cfg.output_file) - 1);
                break;
            case 1016:
                strncpy(mp_cfg.dump_file, optarg, sizeof(mp_cfg.dump_file) - 1);
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
        LOG("[RX] Failed to allocate memory for config");
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
            "      \"heartbeatTimeoutMs\": %d,\n"
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
            "      \"width\": 1920,\n"
            "      \"height\": 1080,\n"
            "      \"fps\": %.1f,\n"
            "      \"pixelFormat\": \"yuv422p10le\"\n"
            "    }\n"
            "  }\n"
            "}",
            mp_cfg.group_name, mp_cfg.node_name, mp_cfg.node_id,
            mp_cfg.group_topology, mp_cfg.max_group_size, mp_cfg.heartbeat_timeout_ms,
            mp_cfg.sync_enabled ? "true" : "false", mp_cfg.frame_rate,
            mp_cfg.priority, mp_cfg.reliability, mp_cfg.frame_rate);
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
            "      \"heartbeatTimeoutMs\": %d,\n"
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
            "      \"channels\": 2,\n"
            "      \"sampleRate\": 48000,\n"
            "      \"format\": \"pcm_s16le\"\n"
            "    }\n"
            "  }\n"
            "}",
            mp_cfg.group_name, mp_cfg.node_name, mp_cfg.node_id,
            mp_cfg.group_topology, mp_cfg.max_group_size, mp_cfg.heartbeat_timeout_ms,
            mp_cfg.sync_enabled ? "true" : "false", mp_cfg.frame_rate,
            mp_cfg.priority, mp_cfg.reliability);
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
            "      \"heartbeatTimeoutMs\": %d,\n"
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
            mp_cfg.group_topology, mp_cfg.max_group_size, mp_cfg.heartbeat_timeout_ms,
            mp_cfg.sync_enabled ? "true" : "false", mp_cfg.frame_rate,
            mp_cfg.priority, mp_cfg.reliability);
    }

    return config;
}

group_member_t* find_or_create_member(uint8_t node_id) {
    if (!mp_cfg.enable_member_tracking) return NULL;
    
    group_member_t *member = &members[node_id];
    
    if (!member->is_active) {
        member->node_id = node_id;
        snprintf(member->node_name, sizeof(member->node_name), "node_%d", node_id);
        member->packets_received = 0;
        member->last_sequence = 0;
        member->packets_lost = 0;
        gettimeofday(&member->last_seen, NULL);
        member->last_heartbeat = member->last_seen;
        member->avg_interval_ms = 0.0;
        member->is_active = 1;
        
        member_count++;
        if (member_count > stats.max_members_seen) {
            stats.max_members_seen = member_count;
        }
        
        LOG("[RX] New group member discovered: Node %d", node_id);
    }
    
    return member;
}

int parse_group_header(uint8_t *buffer, size_t size, group_member_t **member) {
    struct {
        uint32_t magic;
        uint8_t node_id;
        uint64_t sequence;
        uint64_t timestamp_us;
        uint8_t pattern_type;
    } __attribute__((packed)) header;
    
    if (size < sizeof(header)) {
        return 0; // Invalid header
    }
    
    memcpy(&header, buffer, sizeof(header));
    
    if (header.magic != 0x47525550) { // 'GRUP'
        return 0; // Invalid magic
    }
    
    *member = find_or_create_member(header.node_id);
    if (!*member) return 1; // Valid but tracking disabled
    
    // Update member statistics
    struct timeval now;
    gettimeofday(&now, NULL);
    
    double interval_ms = ((now.tv_sec - (*member)->last_seen.tv_sec) * 1000.0) +
                        ((now.tv_usec - (*member)->last_seen.tv_usec) / 1000.0);
    
    if ((*member)->packets_received > 0) {
        (*member)->avg_interval_ms = (((*member)->avg_interval_ms * ((*member)->packets_received - 1)) + interval_ms) / (*member)->packets_received;
    }
    
    // Check for packet loss
    if ((*member)->packets_received > 0 && header.sequence > (*member)->last_sequence + 1) {
        uint64_t lost = header.sequence - (*member)->last_sequence - 1;
        (*member)->packets_lost += lost;
        LOG("[RX] Packet loss detected from node %d: %lu packets", header.node_id, lost);
    }
    
    (*member)->last_sequence = header.sequence;
    (*member)->last_seen = now;
    (*member)->packets_received++;
    
    // Verify pattern if enabled
    if (mp_cfg.enable_pattern_verify && size > sizeof(header)) {
        size_t data_size = size - sizeof(header);
        uint8_t *data_ptr = buffer + sizeof(header);
        int pattern_ok = 1;
        
        if (header.pattern_type == 1) { // sequential
            for (size_t i = 0; i < data_size && pattern_ok; i++) {
                uint8_t expected = (uint8_t)((header.sequence + i) & 0xFF);
                if (data_ptr[i] != expected) {
                    pattern_ok = 0;
                }
            }
        } else if (header.pattern_type == 3) { // broadcast
            uint8_t expected = (uint8_t)(header.sequence & 0xFF);
            for (size_t i = 0; i < data_size && pattern_ok; i++) {
                if (data_ptr[i] != expected) {
                    pattern_ok = 0;
                }
            }
        }
        
        if (!pattern_ok) {
            stats.pattern_errors++;
            LOG("[RX] Pattern verification failed for node %d, sequence %lu", header.node_id, header.sequence);
        }
    }
    
    return 1;
}

void update_member_status() {
    if (!mp_cfg.enable_member_tracking) return;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    stats.active_members = 0;
    
    for (int i = 0; i < 256; i++) {
        if (members[i].is_active) {
            double elapsed_ms = ((now.tv_sec - members[i].last_seen.tv_sec) * 1000.0) +
                              ((now.tv_usec - members[i].last_seen.tv_usec) / 1000.0);
            
            if (elapsed_ms > mp_cfg.heartbeat_timeout_ms) {
                LOG("[RX] Member timeout: Node %d (last seen %.1f ms ago)", i, elapsed_ms);
                members[i].is_active = 0;
                member_count--;
            } else {
                stats.active_members++;
            }
        }
    }
}

void analyze_synchronization(struct timeval *packet_time) {
    if (!mp_cfg.sync_enabled) return;
    
    static struct timeval last_frame_time = {0, 0};
    
    if (last_frame_time.tv_sec == 0) {
        last_frame_time = *packet_time;
        return;
    }
    
    double interval_ms = ((packet_time->tv_sec - last_frame_time.tv_sec) * 1000.0) +
                        ((packet_time->tv_usec - last_frame_time.tv_usec) / 1000.0);
    
    double expected_interval_ms = 1000.0 / mp_cfg.frame_rate;
    double drift = fabs(interval_ms - expected_interval_ms);
    
    // Update average frame interval
    if (stats.total_packets_received > 1) {
        stats.avg_frame_interval_ms = ((stats.avg_frame_interval_ms * (stats.total_packets_received - 2)) + interval_ms) / (stats.total_packets_received - 1);
    }
    
    // Update sync drift
    stats.sync_drift_ms = ((stats.sync_drift_ms * (stats.total_packets_received - 2)) + drift) / (stats.total_packets_received - 1);
    
    // Check for significant sync violations (> 10% of expected interval)
    if (drift > (expected_interval_ms * 0.1)) {
        stats.sync_violations++;
    }
    
    last_frame_time = *packet_time;
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    double throughput_mbps = (stats.total_bytes_received * 8.0) / (elapsed * 1000000.0);
    
    printf("\r[RX] Progress: %.1fs | Packets: %lu | Throughput: %.2f Mbps | Active Members: %d", 
           elapsed, stats.total_packets_received, throughput_mbps, stats.active_members);
    
    if (mp_cfg.sync_enabled) {
        printf(" | Sync Drift: %.2f ms", stats.sync_drift_ms);
    }
    
    if (mp_cfg.enable_pattern_verify && stats.pattern_errors > 0) {
        printf(" | Pattern Errors: %lu", stats.pattern_errors);
    }
    
    fflush(stdout);
}

void save_group_results() {
    if (strlen(mp_cfg.output_file) == 0) return;
    
    FILE *f = fopen(mp_cfg.output_file, "w");
    if (!f) {
        LOG("[RX] Failed to open output file: %s", mp_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Multipoint Group Reception Results\n");
    fprintf(f, "Group Name: %s\n", mp_cfg.group_name);
    fprintf(f, "Node Name: %s\n", mp_cfg.node_name);
    fprintf(f, "Node ID: %d\n", mp_cfg.node_id);
    fprintf(f, "Payload Type: %s\n", mp_cfg.payload_type);
    fprintf(f, "Topology: %s\n", mp_cfg.group_topology);
    fprintf(f, "Max Group Size: %d\n", mp_cfg.max_group_size);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Total Packets Received: %lu\n", stats.total_packets_received);
    fprintf(f, "Total Bytes Received: %lu\n", stats.total_bytes_received);
    fprintf(f, "Heartbeats Received: %lu\n", stats.heartbeats_received);
    fprintf(f, "Timeout Events: %lu\n", stats.timeout_events);
    
    double throughput_mbps = (stats.total_bytes_received * 8.0) / (total_time * 1000000.0);
    fprintf(f, "Average Throughput: %.2f Mbps\n", throughput_mbps);
    
    if (mp_cfg.enable_member_tracking) {
        fprintf(f, "Max Members Seen: %d\n", stats.max_members_seen);
        fprintf(f, "Active Members at End: %d\n", stats.active_members);
        
        fprintf(f, "\nMember Statistics:\n");
        for (int i = 0; i < 256; i++) {
            if (members[i].packets_received > 0) {
                fprintf(f, "Node %d: %lu packets, %lu lost, %.2f ms avg interval\n",
                       i, members[i].packets_received, members[i].packets_lost, members[i].avg_interval_ms);
            }
        }
    }
    
    if (mp_cfg.sync_enabled) {
        fprintf(f, "Synchronization Analysis:\n");
        fprintf(f, "Expected Frame Rate: %.1f fps\n", mp_cfg.frame_rate);
        fprintf(f, "Average Frame Interval: %.2f ms\n", stats.avg_frame_interval_ms);
        double actual_fps = 1000.0 / stats.avg_frame_interval_ms;
        fprintf(f, "Actual Frame Rate: %.1f fps\n", actual_fps);
        fprintf(f, "Average Sync Drift: %.2f ms\n", stats.sync_drift_ms);
        fprintf(f, "Sync Violations: %lu\n", stats.sync_violations);
    }
    
    if (mp_cfg.enable_pattern_verify) {
        fprintf(f, "Pattern Verification:\n");
        fprintf(f, "Pattern Errors: %lu\n", stats.pattern_errors);
        double error_rate = (stats.total_packets_received > 0) ?
                          (double)(stats.pattern_errors * 100) / stats.total_packets_received : 0.0;
        fprintf(f, "Error Rate: %.2f%%\n", error_rate);
    }
    
    fclose(f);
    LOG("[RX] Group test results saved to: %s", mp_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[RX] Starting Multipoint Group Receiver");
    LOG("[RX] Group: '%s', Node: '%s' (ID: %d)", mp_cfg.group_name, mp_cfg.node_name, mp_cfg.node_id);
    LOG("[RX] Topology: %s, Max Size: %d, Payload: %s", 
        mp_cfg.group_topology, mp_cfg.max_group_size, mp_cfg.payload_type);
    LOG("[RX] Tracking: %s, Sync: %s, Verify: %s", 
        mp_cfg.enable_member_tracking ? "enabled" : "disabled",
        mp_cfg.sync_enabled ? "enabled" : "disabled",
        mp_cfg.enable_pattern_verify ? "enabled" : "disabled");

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

    LOG("[RX] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[RX] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[RX] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    
    LOG("[RX] Starting group reception for %d seconds...", mp_cfg.test_duration_sec);
    
    FILE *dump_file = NULL;
    if (strlen(mp_cfg.dump_file) > 0) {
        dump_file = fopen(mp_cfg.dump_file, "wb");
        if (!dump_file) {
            LOG("[RX] Warning: Failed to open dump file: %s", mp_cfg.dump_file);
        }
    }

    // Main reception loop
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
            LOG("[RX] Graceful shutdown requested");
            break;
        }
        
        MeshBuffer *buf;
        err = mesh_get_buffer_timeout(connection, &buf, mp_cfg.timeout_ms);
        
        if (err == MESH_ERR_CONN_CLOSED) {
            LOG("[RX] Connection closed");
            break;
        }
        
        if (err == MESH_ERR_TIMEOUT) {
            stats.timeout_events++;
            update_member_status();
            continue;
        }
        
        if (err) {
            LOG("[RX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
            continue;
        }
        
        gettimeofday(&stats.last_packet_time, NULL);
        stats.total_packets_received++;
        stats.total_bytes_received += buf->payload_len;
        
        // Parse group header and track member
        group_member_t *member = NULL;
        parse_group_header((uint8_t*)buf->payload_ptr, buf->payload_len, &member);
        
        // Analyze synchronization
        analyze_synchronization(&stats.last_packet_time);
        
        // Dump data if requested
        if (dump_file) {
            fwrite(buf->payload_ptr, 1, buf->payload_len, dump_file);
        }
        
        err = mesh_put_buffer(&buf);
        if (err) {
            LOG("[RX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
        }
        
        // Print progress every 100 packets
        if (stats.total_packets_received % 100 == 0) {
            update_member_status();
            print_progress_stats();
        }
    }
    
    if (dump_file) {
        fclose(dump_file);
        LOG("[RX] Data dumped to: %s", mp_cfg.dump_file);
    }
    
    printf("\n");
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[RX] Group reception completed in %.2f seconds", total_time);
    LOG("[RX] Total packets received: %lu", stats.total_packets_received);
    LOG("[RX] Total bytes received: %lu", stats.total_bytes_received);
    
    double throughput_mbps = (stats.total_bytes_received * 8.0) / (total_time * 1000000.0);
    LOG("[RX] Average throughput: %.2f Mbps", throughput_mbps);
    
    if (mp_cfg.enable_member_tracking) {
        LOG("[RX] Max group members seen: %d", stats.max_members_seen);
        LOG("[RX] Active members at end: %d", stats.active_members);
        
        uint64_t total_lost = 0;
        for (int i = 0; i < 256; i++) {
            if (members[i].packets_received > 0) {
                total_lost += members[i].packets_lost;
            }
        }
        LOG("[RX] Total packets lost across all members: %lu", total_lost);
    }
    
    if (mp_cfg.sync_enabled) {
        double actual_fps = (stats.avg_frame_interval_ms > 0) ? 1000.0 / stats.avg_frame_interval_ms : 0.0;
        LOG("[RX] Sync analysis - Target: %.1f fps, Actual: %.1f fps, Drift: %.2f ms", 
            mp_cfg.frame_rate, actual_fps, stats.sync_drift_ms);
        LOG("[RX] Sync violations: %lu", stats.sync_violations);
    }
    
    if (mp_cfg.enable_pattern_verify) {
        double error_rate = (stats.total_packets_received > 0) ?
                          (double)(stats.pattern_errors * 100) / stats.total_packets_received : 0.0;
        LOG("[RX] Pattern verification - Errors: %lu (%.2f%%)", stats.pattern_errors, error_rate);
    }
    
    // Save results to file if specified
    save_group_results();

safe_exit:
    LOG("[RX] Shutting down group connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[RX] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    return err;
}
