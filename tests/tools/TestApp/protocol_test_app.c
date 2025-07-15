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
#include <errno.h>
#include <arpa/inet.h>
#include <math.h>
#include <float.h>

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

// Protocol test configuration
typedef struct {
    char protocol_type[32];      // "udp", "tcp", "rdma", "custom", "multicast", "all"
    char transport_mode[16];     // "unicast", "multicast", "broadcast"
    char payload_type[16];       // "video", "audio", "blob", "mixed"
    int test_duration_sec;
    int packet_sizes[8];         // Different packet sizes to test
    int num_packet_sizes;
    int bandwidth_mbps;          // Target bandwidth
    int test_fragmentation;      // Test packet fragmentation
    int test_reordering;         // Test packet reordering
    int test_duplication;        // Test packet duplication
    int validate_checksums;      // Validate packet checksums
    int enable_encryption;       // Test encrypted protocols
    char custom_headers[256];    // Custom protocol headers
    char output_file[256];
    int verbose;
} protocol_config_t;

static protocol_config_t prot_cfg = {
    .protocol_type = "udp",
    .transport_mode = "unicast",
    .payload_type = "video",
    .test_duration_sec = 300,
    .packet_sizes = {1500, 4096, 8192, 16384, 32768, 65536, 131072, 262144},
    .num_packet_sizes = 8,
    .bandwidth_mbps = 100,
    .test_fragmentation = 1,
    .test_reordering = 0,
    .test_duplication = 0,
    .validate_checksums = 1,
    .enable_encryption = 0,
    .custom_headers = "",
    .output_file = "",
    .verbose = 0
};

// Protocol testing statistics
typedef struct {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t packets_lost;
    uint64_t packets_corrupted;
    uint64_t packets_reordered;
    uint64_t packets_duplicated;
    uint64_t fragmented_packets;
    uint64_t checksum_errors;
    uint64_t protocol_errors;
    uint64_t bytes_transmitted;
    uint64_t bytes_received;
    double min_latency_ms;
    double max_latency_ms;
    double avg_latency_ms;
    double jitter_ms;
    double throughput_mbps;
    struct timeval start_time;
    uint32_t current_seq_num;
    uint32_t expected_seq_num;
} protocol_stats_t;

static protocol_stats_t stats = {0};

// Packet header for protocol testing
typedef struct {
    uint32_t magic;              // 0xDEADBEEF
    uint32_t sequence;
    uint32_t timestamp_sec;
    uint32_t timestamp_usec;
    uint32_t packet_size;
    uint32_t checksum;
    uint16_t protocol_version;
    uint16_t flags;
    char protocol_name[16];
} __attribute__((packed)) test_packet_header_t;

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Protocol and Transport Testing Application\n\n");
    printf("Protocol Configuration:\n");
    printf("  -p, --protocol <type>      Protocol type: udp, tcp, rdma, custom, multicast, all (default: %s)\n", prot_cfg.protocol_type);
    printf("  -m, --mode <type>          Transport mode: unicast, multicast, broadcast (default: %s)\n", prot_cfg.transport_mode);
    printf("  --payload <type>           Payload type: video, audio, blob, mixed (default: %s)\n", prot_cfg.payload_type);
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", prot_cfg.test_duration_sec);
    printf("\n  Packet Testing:\n");
    printf("  --packet-sizes <sizes>     Comma-separated packet sizes to test (default: 1500,4096,8192...)\n");
    printf("  --bandwidth <mbps>         Target bandwidth in Mbps (default: %d)\n", prot_cfg.bandwidth_mbps);
    printf("  --test-fragmentation       Enable fragmentation testing (default: %s)\n", prot_cfg.test_fragmentation ? "enabled" : "disabled");
    printf("  --test-reordering          Enable reordering testing\n");
    printf("  --test-duplication         Enable duplication testing\n");
    printf("  --no-fragmentation         Disable fragmentation testing\n");
    printf("  --no-reordering            Disable reordering testing\n");
    printf("  --no-duplication           Disable duplication testing\n");
    printf("\n  Validation:\n");
    printf("  --validate-checksums       Enable checksum validation (default: %s)\n", prot_cfg.validate_checksums ? "enabled" : "disabled");
    printf("  --enable-encryption        Enable encryption testing\n");
    printf("  --custom-headers <headers> Custom protocol headers\n");
    printf("  --no-checksums             Disable checksum validation\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Test all UDP packet sizes\n");
    printf("    %s --protocol udp --test-fragmentation\n", prog_name);
    printf("\n    # Test multicast with encryption\n");
    printf("    %s --protocol multicast --enable-encryption\n", prog_name);
    printf("\n    # Test custom protocol with reordering\n");
    printf("    %s --protocol custom --test-reordering --custom-headers \"CustomProto: v1.0\"\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"protocol", required_argument, 0, 'p'},
        {"mode", required_argument, 0, 'm'},
        {"payload", required_argument, 0, 1001},
        {"test-duration", required_argument, 0, 't'},
        {"packet-sizes", required_argument, 0, 1002},
        {"bandwidth", required_argument, 0, 1003},
        {"test-fragmentation", no_argument, 0, 1004},
        {"test-reordering", no_argument, 0, 1005},
        {"test-duplication", no_argument, 0, 1006},
        {"no-fragmentation", no_argument, 0, 1007},
        {"no-reordering", no_argument, 0, 1008},
        {"no-duplication", no_argument, 0, 1009},
        {"validate-checksums", no_argument, 0, 1010},
        {"enable-encryption", no_argument, 0, 1011},
        {"custom-headers", required_argument, 0, 1012},
        {"no-checksums", no_argument, 0, 1013},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "p:m:t:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'p':
                strncpy(prot_cfg.protocol_type, optarg, sizeof(prot_cfg.protocol_type) - 1);
                break;
            case 'm':
                strncpy(prot_cfg.transport_mode, optarg, sizeof(prot_cfg.transport_mode) - 1);
                break;
            case 1001:
                strncpy(prot_cfg.payload_type, optarg, sizeof(prot_cfg.payload_type) - 1);
                break;
            case 't':
                prot_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1002: {
                // Parse comma-separated packet sizes
                char *token = strtok(optarg, ",");
                prot_cfg.num_packet_sizes = 0;
                while (token && prot_cfg.num_packet_sizes < 8) {
                    prot_cfg.packet_sizes[prot_cfg.num_packet_sizes++] = atoi(token);
                    token = strtok(NULL, ",");
                }
                break;
            }
            case 1003:
                prot_cfg.bandwidth_mbps = atoi(optarg);
                break;
            case 1004:
                prot_cfg.test_fragmentation = 1;
                break;
            case 1005:
                prot_cfg.test_reordering = 1;
                break;
            case 1006:
                prot_cfg.test_duplication = 1;
                break;
            case 1007:
                prot_cfg.test_fragmentation = 0;
                break;
            case 1008:
                prot_cfg.test_reordering = 0;
                break;
            case 1009:
                prot_cfg.test_duplication = 0;
                break;
            case 1010:
                prot_cfg.validate_checksums = 1;
                break;
            case 1011:
                prot_cfg.enable_encryption = 1;
                break;
            case 1012:
                strncpy(prot_cfg.custom_headers, optarg, sizeof(prot_cfg.custom_headers) - 1);
                break;
            case 1013:
                prot_cfg.validate_checksums = 0;
                break;
            case 'o':
                strncpy(prot_cfg.output_file, optarg, sizeof(prot_cfg.output_file) - 1);
                break;
            case 'v':
                prot_cfg.verbose = 1;
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

// Calculate simple checksum
uint32_t calculate_checksum(const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0;
    
    for (size_t i = 0; i < length; i++) {
        sum += bytes[i];
    }
    
    return sum;
}

// Create test packet with header
int create_test_packet(MeshBuffer *buf, uint32_t sequence, int packet_size) {
    if (!buf || packet_size < sizeof(test_packet_header_t)) {
        return -1;
    }
    
    test_packet_header_t *header = (test_packet_header_t *)buf->payload_ptr;
    memset(header, 0, sizeof(test_packet_header_t));
    
    // Fill header
    header->magic = htonl(0xDEADBEEF);
    header->sequence = htonl(sequence);
    
    struct timeval now;
    gettimeofday(&now, NULL);
    header->timestamp_sec = htonl(now.tv_sec);
    header->timestamp_usec = htonl(now.tv_usec);
    header->packet_size = htonl(packet_size);
    header->protocol_version = htons(1);
    header->flags = 0;
    
    strncpy(header->protocol_name, prot_cfg.protocol_type, sizeof(header->protocol_name) - 1);
    
    // Fill payload
    uint8_t *payload = (uint8_t *)buf->payload_ptr + sizeof(test_packet_header_t);
    int payload_size = packet_size - sizeof(test_packet_header_t);
    
    if (strcmp(prot_cfg.payload_type, "video") == 0) {
        // Video-like pattern
        for (int i = 0; i < payload_size; i++) {
            payload[i] = (sequence + i) & 0xFF;
        }
    } else if (strcmp(prot_cfg.payload_type, "audio") == 0) {
        // Audio-like pattern (sine wave)
        for (int i = 0; i < payload_size; i++) {
            payload[i] = (uint8_t)(128 + 127 * sin(2.0 * M_PI * i / 48));
        }
    } else {
        // Blob pattern
        for (int i = 0; i < payload_size; i++) {
            payload[i] = rand() & 0xFF;
        }
    }
    
    // Calculate checksum if enabled
    if (prot_cfg.validate_checksums) {
        header->checksum = htonl(calculate_checksum(payload, payload_size));
    }
    
    // Set the payload length using the mesh API function
    mesh_buffer_set_payload_len(buf, packet_size);
    return 0;
}

// Validate received packet
int validate_packet(MeshBuffer *buf) {
    if (!buf || buf->payload_len < sizeof(test_packet_header_t)) {
        stats.packets_corrupted++;
        return -1;
    }
    
    test_packet_header_t *header = (test_packet_header_t *)buf->payload_ptr;
    
    // Validate magic number
    if (ntohl(header->magic) != 0xDEADBEEF) {
        stats.packets_corrupted++;
        return -1;
    }
    
    uint32_t sequence = ntohl(header->sequence);
    uint32_t packet_size = ntohl(header->packet_size);
    
    if (packet_size != buf->payload_len) {
        stats.packets_corrupted++;
        return -1;
    }
    
    // Check for reordering
    if (sequence < stats.expected_seq_num) {
        stats.packets_reordered++;
        if (prot_cfg.verbose) {
            LOG("[PROT] Packet reordering detected: got %u, expected %u", sequence, stats.expected_seq_num);
        }
    } else if (sequence > stats.expected_seq_num) {
        // Packets lost
        stats.packets_lost += (sequence - stats.expected_seq_num);
        stats.expected_seq_num = sequence + 1;
    } else {
        stats.expected_seq_num = sequence + 1;
    }
    
    // Validate checksum if enabled
    if (prot_cfg.validate_checksums) {
        uint32_t stored_checksum = ntohl(header->checksum);
        uint8_t *payload = (uint8_t *)buf->payload_ptr + sizeof(test_packet_header_t);
        int payload_size = packet_size - sizeof(test_packet_header_t);
        uint32_t calculated_checksum = calculate_checksum(payload, payload_size);
        
        if (stored_checksum != calculated_checksum) {
            stats.checksum_errors++;
            if (prot_cfg.verbose) {
                LOG("[PROT] Checksum error: stored=%u, calculated=%u", stored_checksum, calculated_checksum);
            }
            return -1;
        }
    }
    
    // Calculate latency
    struct timeval now, packet_time;
    gettimeofday(&now, NULL);
    packet_time.tv_sec = ntohl(header->timestamp_sec);
    packet_time.tv_usec = ntohl(header->timestamp_usec);
    
    double latency_ms = ((now.tv_sec - packet_time.tv_sec) * 1000.0) +
                       ((now.tv_usec - packet_time.tv_usec) / 1000.0);
    
    if (stats.packets_received == 0) {
        stats.min_latency_ms = stats.max_latency_ms = stats.avg_latency_ms = latency_ms;
    } else {
        if (latency_ms < stats.min_latency_ms) stats.min_latency_ms = latency_ms;
        if (latency_ms > stats.max_latency_ms) stats.max_latency_ms = latency_ms;
        stats.avg_latency_ms = ((stats.avg_latency_ms * stats.packets_received) + latency_ms) / (stats.packets_received + 1);
    }
    
    stats.packets_received++;
    stats.bytes_received += packet_size;
    
    return 0;
}

char* generate_protocol_config() {
    char *config = malloc(2048);
    if (!config) {
        LOG("[PROT] Failed to allocate memory for config");
        return NULL;
    }

    const char *protocol_specific = "";
    
    if (strcmp(prot_cfg.protocol_type, "udp") == 0) {
        protocol_specific = 
            "    \"udp\": {\n"
            "      \"bufferSize\": 65536,\n"
            "      \"socketOptions\": {\n"
            "        \"SO_REUSEADDR\": true,\n"
            "        \"SO_RCVBUF\": 1048576,\n"
            "        \"SO_SNDBUF\": 1048576\n"
            "      }\n"
            "    },\n";
    } else if (strcmp(prot_cfg.protocol_type, "tcp") == 0) {
        protocol_specific = 
            "    \"tcp\": {\n"
            "      \"keepAlive\": true,\n"
            "      \"noDelay\": true,\n"
            "      \"bufferSize\": 131072,\n"
            "      \"connectionTimeout\": 30000\n"
            "    },\n";
    } else if (strcmp(prot_cfg.protocol_type, "rdma") == 0) {
        protocol_specific = 
            "    \"rdma\": {\n"
            "      \"provider\": \"verbs\",\n"
            "      \"queueDepth\": 1024,\n"
            "      \"completionQueueSize\": 2048,\n"
            "      \"maxInlineData\": 256\n"
            "    },\n";
    } else if (strcmp(prot_cfg.protocol_type, "multicast") == 0) {
        protocol_specific = 
            "    \"multicast\": {\n"
            "      \"group\": \"239.255.1.1\",\n"
            "      \"ttl\": 64,\n"
            "      \"loopback\": false,\n"
            "      \"interface\": \"0.0.0.0\"\n"
            "    },\n";
    }

    snprintf(config, 2048,
        "{\n"
        "  \"connection\": {\n"
        "    \"protocol\": \"%s\",\n"
        "    \"transport\": \"%s\",\n"
        "    \"encryption\": %s,\n"
        "    \"validation\": {\n"
        "      \"checksums\": %s,\n"
        "      \"sequencing\": true,\n"
        "      \"fragmentation\": %s\n"
        "    },\n"
        "%s"
        "    \"customHeaders\": \"%s\"\n"
        "  },\n"
        "  \"payload\": {\n"
        "    \"type\": \"%s\",\n"
        "    \"maxSize\": %d,\n"
        "    \"targetBandwidth\": %d\n"
        "  },\n"
        "  \"testing\": {\n"
        "    \"reordering\": %s,\n"
        "    \"duplication\": %s,\n"
        "    \"fragmentation\": %s\n"
        "  }\n"
        "}",
        prot_cfg.protocol_type,
        prot_cfg.transport_mode,
        prot_cfg.enable_encryption ? "true" : "false",
        prot_cfg.validate_checksums ? "true" : "false",
        prot_cfg.test_fragmentation ? "true" : "false",
        protocol_specific,
        prot_cfg.custom_headers,
        prot_cfg.payload_type,
        prot_cfg.packet_sizes[prot_cfg.num_packet_sizes - 1],
        prot_cfg.bandwidth_mbps,
        prot_cfg.test_reordering ? "true" : "false",
        prot_cfg.test_duplication ? "true" : "false",
        prot_cfg.test_fragmentation ? "true" : "false");

    return config;
}

int test_packet_size(int packet_size) {
    LOG("[PROT] Testing packet size: %d bytes", packet_size);
    
    uint64_t packets_sent_start = stats.packets_sent;
    uint64_t packets_received_start = stats.packets_received;
    
    struct timeval test_start, test_end;
    gettimeofday(&test_start, NULL);
    test_end = test_start;
    test_end.tv_sec += 30; // 30 second test per packet size
    
    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        if (shutdown_flag == SHUTDOWN_REQUESTED) {
            break;
        }
        
        // Send packet
        MeshBuffer *buf;
        int err = mesh_get_buffer(connection, &buf);
        if (err == 0 && buf) {
            if (create_test_packet(buf, stats.current_seq_num++, packet_size) == 0) {
                err = mesh_put_buffer(&buf);
                if (err == 0) {
                    stats.packets_sent++;
                    stats.bytes_transmitted += packet_size;
                }
            }
        }
        
        // Receive packets
        err = mesh_get_buffer_timeout(connection, &buf, 1); // 1ms timeout
        if (err == 0 && buf) {
            validate_packet(buf);
            mesh_put_buffer(&buf);
        }
        
        // Bandwidth control
        usleep(1000); // 1ms
    }
    
    uint64_t packets_sent_this_test = stats.packets_sent - packets_sent_start;
    uint64_t packets_received_this_test = stats.packets_received - packets_received_start;
    
    double loss_rate = 0.0;
    if (packets_sent_this_test > 0) {
        loss_rate = ((double)(packets_sent_this_test - packets_received_this_test) / packets_sent_this_test) * 100.0;
    }
    
    LOG("[PROT] Packet size %d: sent=%lu, received=%lu, loss=%.2f%%", 
        packet_size, packets_sent_this_test, packets_received_this_test, loss_rate);
    
    return 0;
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    if (elapsed > 0) {
        stats.throughput_mbps = (stats.bytes_transmitted * 8.0) / (elapsed * 1000000.0);
    }
    
    double loss_rate = 0.0;
    if (stats.packets_sent > 0) {
        loss_rate = ((double)(stats.packets_sent - stats.packets_received) / stats.packets_sent) * 100.0;
    }
    
    printf("\r[PROT] %.1fs | Protocol: %s | Sent: %lu | Rcvd: %lu | Loss: %.2f%% | Tput: %.1f Mbps", 
           elapsed, prot_cfg.protocol_type, stats.packets_sent, stats.packets_received, 
           loss_rate, stats.throughput_mbps);
    fflush(stdout);
}

void save_protocol_results() {
    if (strlen(prot_cfg.output_file) == 0) return;
    
    FILE *f = fopen(prot_cfg.output_file, "w");
    if (!f) {
        LOG("[PROT] Failed to open output file: %s", prot_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Protocol and Transport Test Results\n");
    fprintf(f, "Protocol: %s\n", prot_cfg.protocol_type);
    fprintf(f, "Transport Mode: %s\n", prot_cfg.transport_mode);
    fprintf(f, "Payload Type: %s\n", prot_cfg.payload_type);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Target Bandwidth: %d Mbps\n", prot_cfg.bandwidth_mbps);
    fprintf(f, "Encryption: %s\n", prot_cfg.enable_encryption ? "enabled" : "disabled");
    
    fprintf(f, "\nPacket Statistics:\n");
    fprintf(f, "Packets Sent: %lu\n", stats.packets_sent);
    fprintf(f, "Packets Received: %lu\n", stats.packets_received);
    fprintf(f, "Packets Lost: %lu\n", stats.packets_lost);
    fprintf(f, "Packets Corrupted: %lu\n", stats.packets_corrupted);
    fprintf(f, "Packets Reordered: %lu\n", stats.packets_reordered);
    fprintf(f, "Packets Duplicated: %lu\n", stats.packets_duplicated);
    fprintf(f, "Fragmented Packets: %lu\n", stats.fragmented_packets);
    
    if (stats.packets_sent > 0) {
        double loss_rate = ((double)(stats.packets_sent - stats.packets_received) / stats.packets_sent) * 100.0;
        fprintf(f, "Packet Loss Rate: %.3f%%\n", loss_rate);
    }
    
    fprintf(f, "\nThroughput Statistics:\n");
    fprintf(f, "Bytes Transmitted: %lu\n", stats.bytes_transmitted);
    fprintf(f, "Bytes Received: %lu\n", stats.bytes_received);
    fprintf(f, "Average Throughput: %.2f Mbps\n", stats.throughput_mbps);
    
    if (stats.packets_received > 0) {
        fprintf(f, "\nLatency Statistics:\n");
        fprintf(f, "Minimum Latency: %.3f ms\n", stats.min_latency_ms);
        fprintf(f, "Maximum Latency: %.3f ms\n", stats.max_latency_ms);
        fprintf(f, "Average Latency: %.3f ms\n", stats.avg_latency_ms);
        fprintf(f, "Jitter: %.3f ms\n", stats.jitter_ms);
    }
    
    fprintf(f, "\nValidation Results:\n");
    fprintf(f, "Checksum Errors: %lu\n", stats.checksum_errors);
    fprintf(f, "Protocol Errors: %lu\n", stats.protocol_errors);
    
    fclose(f);
    LOG("[PROT] Test results saved to: %s", prot_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[PROT] Starting Protocol and Transport Test");
    LOG("[PROT] Protocol: %s, Mode: %s, Payload: %s, Duration: %d seconds", 
        prot_cfg.protocol_type, prot_cfg.transport_mode, prot_cfg.payload_type, prot_cfg.test_duration_sec);
    LOG("[PROT] Testing %d packet sizes, Target bandwidth: %d Mbps", 
        prot_cfg.num_packet_sizes, prot_cfg.bandwidth_mbps);
    LOG("[PROT] Features - Fragmentation: %s, Reordering: %s, Checksums: %s, Encryption: %s", 
        prot_cfg.test_fragmentation ? "enabled" : "disabled",
        prot_cfg.test_reordering ? "enabled" : "disabled",
        prot_cfg.validate_checksums ? "enabled" : "disabled",
        prot_cfg.enable_encryption ? "enabled" : "disabled");

    // Initialize random seed
    srand(time(NULL));

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate protocol-specific connection configuration
    conn_cfg = generate_protocol_config();
    if (!conn_cfg) {
        free(client_cfg);
        exit(EXIT_FAILURE);
    }

    LOG("[PROT] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[PROT] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[PROT] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    stats.min_latency_ms = DBL_MAX;
    
    LOG("[PROT] Starting protocol test for %d seconds...", prot_cfg.test_duration_sec);

    // Test each packet size
    for (int i = 0; i < prot_cfg.num_packet_sizes; i++) {
        if (shutdown_flag == SHUTDOWN_REQUESTED) break;
        
        test_packet_size(prot_cfg.packet_sizes[i]);
        
        // Progress update
        print_progress_stats();
    }
    
    printf("\n");
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[PROT] Test completed in %.2f seconds", total_time);
    LOG("[PROT] Total packets sent: %lu", stats.packets_sent);
    LOG("[PROT] Total packets received: %lu", stats.packets_received);
    LOG("[PROT] Average throughput: %.2f Mbps", stats.throughput_mbps);
    
    if (stats.packets_sent > 0) {
        double loss_rate = ((double)(stats.packets_sent - stats.packets_received) / stats.packets_sent) * 100.0;
        LOG("[PROT] Packet loss rate: %.3f%%", loss_rate);
    }
    
    if (stats.packets_received > 0) {
        LOG("[PROT] Latency - Min: %.3f ms, Max: %.3f ms, Avg: %.3f ms", 
            stats.min_latency_ms, stats.max_latency_ms, stats.avg_latency_ms);
    }
    
    LOG("[PROT] Errors - Checksum: %lu, Protocol: %lu, Corrupted: %lu", 
        stats.checksum_errors, stats.protocol_errors, stats.packets_corrupted);
    
    // Save results to file if specified
    save_protocol_results();

safe_exit:
    LOG("[PROT] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[PROT] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    return err;
}
