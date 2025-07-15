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

// RDMA test configuration
typedef struct {
    char rdma_provider[16];      // "tcp", "verbs"
    int num_endpoints;           // 1-8
    int buffer_queue_capacity;   // Buffer pool size
    int conn_delay_ms;          // Connection creation delay
    char payload_type[16];       // "video", "audio", "blob"
    int test_duration_sec;       // Test duration in seconds
    int timeout_ms;             // Receive timeout
    char test_pattern[32];       // "sequential", "random", "zero"
    int enable_latency_test;     // Enable latency measurements
    int enable_throughput_test;  // Enable throughput measurements
    int enable_pattern_verify;   // Verify received pattern
    int enable_packet_loss;      // Track packet loss
    char output_file[256];       // Output file for test results
    char dump_file[256];         // File to dump received data
} rdma_test_config_t;

static rdma_test_config_t rdma_cfg = {
    .rdma_provider = "tcp",
    .num_endpoints = 1,
    .buffer_queue_capacity = 16,
    .conn_delay_ms = 0,
    .payload_type = "blob",
    .test_duration_sec = 30,
    .timeout_ms = 1000,
    .test_pattern = "sequential",
    .enable_latency_test = 1,
    .enable_throughput_test = 1,
    .enable_pattern_verify = 0,
    .enable_packet_loss = 1,
    .output_file = "",
    .dump_file = ""
};

// Test statistics
typedef struct {
    uint64_t packets_received;
    uint64_t bytes_received;
    uint64_t packets_lost;
    uint64_t packets_corrupted;
    uint64_t expected_packet_num;
    double min_latency_us;
    double max_latency_us;
    double avg_latency_us;
    double throughput_mbps;
    struct timeval start_time;
    struct timeval end_time;
    struct timeval last_packet_time;
} test_stats_t;

static test_stats_t stats = {0};

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("RDMA Configuration Receiver Test Application\n\n");
    printf("Options:\n");
    printf("  --rdma-provider <prov>     RDMA provider: tcp, verbs (default: %s)\n", rdma_cfg.rdma_provider);
    printf("  --rdma-endpoints <num>     Number of RDMA endpoints 1-8 (default: %d)\n", rdma_cfg.num_endpoints);
    printf("  -q, --queue-capacity <num> Buffer queue capacity (default: %d)\n", rdma_cfg.buffer_queue_capacity);
    printf("  -d, --delay <ms>           Connection creation delay in ms (default: %d)\n", rdma_cfg.conn_delay_ms);
    printf("\n  Test Configuration:\n");
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", rdma_cfg.test_duration_sec);
    printf("  --timeout <ms>             Receive timeout in milliseconds (default: %d)\n", rdma_cfg.timeout_ms);
    printf("  --pattern <type>           Expected test pattern: sequential, random, zero (default: %s)\n", rdma_cfg.test_pattern);
    printf("  --payload-type <type>      Payload type: video, audio, blob (default: %s)\n", rdma_cfg.payload_type);
    printf("\n  Performance Measurements:\n");
    printf("  --enable-latency           Enable latency measurements (default: %s)\n", rdma_cfg.enable_latency_test ? "enabled" : "disabled");
    printf("  --disable-latency          Disable latency measurements\n");
    printf("  --enable-throughput        Enable throughput measurements (default: %s)\n", rdma_cfg.enable_throughput_test ? "enabled" : "disabled");
    printf("  --disable-throughput       Disable throughput measurements\n");
    printf("  --enable-verify            Enable pattern verification\n");
    printf("  --disable-verify           Disable pattern verification (default)\n");
    printf("  --enable-loss              Enable packet loss tracking (default: %s)\n", rdma_cfg.enable_packet_loss ? "enabled" : "disabled");
    printf("  --disable-loss             Disable packet loss tracking\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  --dump <file>              Dump received data to file\n");
    printf("\n  General:\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Basic TCP RDMA test\n");
    printf("    %s --rdma-provider tcp --rdma-endpoints 2\n", prog_name);
    printf("\n    # High-performance verbs test with pattern verification\n");
    printf("    %s --rdma-provider verbs --rdma-endpoints 8 --enable-verify\n", prog_name);
    printf("\n    # Data integrity test with packet loss detection\n");
    printf("    %s --enable-loss --enable-verify --pattern sequential\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"rdma-provider", required_argument, 0, 1001},
        {"rdma-endpoints", required_argument, 0, 1002},
        {"queue-capacity", required_argument, 0, 'q'},
        {"delay", required_argument, 0, 'd'},
        {"test-duration", required_argument, 0, 't'},
        {"timeout", required_argument, 0, 1003},
        {"pattern", required_argument, 0, 1004},
        {"payload-type", required_argument, 0, 1005},
        {"enable-latency", no_argument, 0, 1006},
        {"disable-latency", no_argument, 0, 1007},
        {"enable-throughput", no_argument, 0, 1008},
        {"disable-throughput", no_argument, 0, 1009},
        {"enable-verify", no_argument, 0, 1010},
        {"disable-verify", no_argument, 0, 1011},
        {"enable-loss", no_argument, 0, 1012},
        {"disable-loss", no_argument, 0, 1013},
        {"output", required_argument, 0, 'o'},
        {"dump", required_argument, 0, 1014},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "q:d:t:o:h", long_options, NULL)) != -1) {
        switch (c) {
            case 1001:
                strncpy(rdma_cfg.rdma_provider, optarg, sizeof(rdma_cfg.rdma_provider) - 1);
                break;
            case 1002:
                rdma_cfg.num_endpoints = atoi(optarg);
                if (rdma_cfg.num_endpoints < 1 || rdma_cfg.num_endpoints > 8) {
                    fprintf(stderr, "Error: Number of endpoints must be between 1 and 8\n");
                    exit(1);
                }
                break;
            case 'q':
                rdma_cfg.buffer_queue_capacity = atoi(optarg);
                break;
            case 'd':
                rdma_cfg.conn_delay_ms = atoi(optarg);
                break;
            case 't':
                rdma_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1003:
                rdma_cfg.timeout_ms = atoi(optarg);
                break;
            case 1004:
                strncpy(rdma_cfg.test_pattern, optarg, sizeof(rdma_cfg.test_pattern) - 1);
                break;
            case 1005:
                strncpy(rdma_cfg.payload_type, optarg, sizeof(rdma_cfg.payload_type) - 1);
                break;
            case 1006:
                rdma_cfg.enable_latency_test = 1;
                break;
            case 1007:
                rdma_cfg.enable_latency_test = 0;
                break;
            case 1008:
                rdma_cfg.enable_throughput_test = 1;
                break;
            case 1009:
                rdma_cfg.enable_throughput_test = 0;
                break;
            case 1010:
                rdma_cfg.enable_pattern_verify = 1;
                break;
            case 1011:
                rdma_cfg.enable_pattern_verify = 0;
                break;
            case 1012:
                rdma_cfg.enable_packet_loss = 1;
                break;
            case 1013:
                rdma_cfg.enable_packet_loss = 0;
                break;
            case 'o':
                strncpy(rdma_cfg.output_file, optarg, sizeof(rdma_cfg.output_file) - 1);
                break;
            case 1014:
                strncpy(rdma_cfg.dump_file, optarg, sizeof(rdma_cfg.dump_file) - 1);
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

char* generate_rdma_config() {
    char *config = malloc(1024);
    if (!config) {
        LOG("[RX] Failed to allocate memory for config");
        return NULL;
    }

    if (strcmp(rdma_cfg.payload_type, "video") == 0) {
        snprintf(config, 1024,
            "{\n"
            "  \"bufferQueueCapacity\": %d,\n"
            "  \"connCreationDelayMilliseconds\": %d,\n"
            "  \"connection\": {\n"
            "    \"memif\": {\n"
            "      \"interface\": \"rx_memif\",\n"
            "      \"socketPath\": \"/run/mcm/mcm_rx_memif.sock\"\n"
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
            "      \"width\": 1920,\n"
            "      \"height\": 1080,\n"
            "      \"fps\": 30.0,\n"
            "      \"pixelFormat\": \"yuv422p10le\"\n"
            "    }\n"
            "  }\n"
            "}",
            rdma_cfg.buffer_queue_capacity, rdma_cfg.conn_delay_ms,
            rdma_cfg.rdma_provider, rdma_cfg.num_endpoints);
    } else if (strcmp(rdma_cfg.payload_type, "audio") == 0) {
        snprintf(config, 1024,
            "{\n"
            "  \"bufferQueueCapacity\": %d,\n"
            "  \"connCreationDelayMilliseconds\": %d,\n"
            "  \"connection\": {\n"
            "    \"memif\": {\n"
            "      \"interface\": \"rx_memif\",\n"
            "      \"socketPath\": \"/run/mcm/mcm_rx_memif.sock\"\n"
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
            "      \"channels\": 2,\n"
            "      \"sampleRate\": 48000,\n"
            "      \"format\": \"pcm_s16le\"\n"
            "    }\n"
            "  }\n"
            "}",
            rdma_cfg.buffer_queue_capacity, rdma_cfg.conn_delay_ms,
            rdma_cfg.rdma_provider, rdma_cfg.num_endpoints);
    } else {
        // Blob configuration
        snprintf(config, 1024,
            "{\n"
            "  \"bufferQueueCapacity\": %d,\n"
            "  \"connCreationDelayMilliseconds\": %d,\n"
            "  \"connection\": {\n"
            "    \"memif\": {\n"
            "      \"interface\": \"rx_memif\",\n"
            "      \"socketPath\": \"/run/mcm/mcm_rx_memif.sock\"\n"
            "    }\n"
            "  },\n"
            "  \"options\": {\n"
            "    \"rdma\": {\n"
            "      \"provider\": \"%s\",\n"
            "      \"numEndpoints\": %d\n"
            "    }\n"
            "  },\n"
            "  \"payload\": {\n"
            "    \"blob\": {}\n"
            "  }\n"
            "}",
            rdma_cfg.buffer_queue_capacity, rdma_cfg.conn_delay_ms,
            rdma_cfg.rdma_provider, rdma_cfg.num_endpoints);
    }

    return config;
}

int verify_test_pattern(uint8_t *buffer, size_t size, const char *pattern, uint64_t expected_packet_num) {
    if (strcmp(pattern, "sequential") == 0) {
        for (size_t i = sizeof(struct timeval); i < size; i++) {
            uint8_t expected = (uint8_t)((expected_packet_num + i) & 0xFF);
            if (buffer[i] != expected) {
                return 0; // Pattern mismatch
            }
        }
    } else if (strcmp(pattern, "zero") == 0) {
        for (size_t i = sizeof(struct timeval); i < size; i++) {
            if (buffer[i] != 0) {
                return 0; // Pattern mismatch
            }
        }
    }
    // Random pattern verification not implemented (would need shared seed)
    
    return 1; // Pattern matches
}

double calculate_elapsed_time(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec) / 1000000.0;
}

void update_latency_stats(struct timeval *sent_time) {
    if (!rdma_cfg.enable_latency_test) return;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    double latency_us = ((now.tv_sec - sent_time->tv_sec) * 1000000.0) + 
                       (now.tv_usec - sent_time->tv_usec);
    
    if (stats.packets_received == 1) {
        stats.min_latency_us = stats.max_latency_us = latency_us;
        stats.avg_latency_us = latency_us;
    } else {
        if (latency_us < stats.min_latency_us) stats.min_latency_us = latency_us;
        if (latency_us > stats.max_latency_us) stats.max_latency_us = latency_us;
        stats.avg_latency_us = ((stats.avg_latency_us * (stats.packets_received - 1)) + latency_us) / stats.packets_received;
    }
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = calculate_elapsed_time(&stats.start_time, &now);
    
    if (rdma_cfg.enable_throughput_test) {
        stats.throughput_mbps = (stats.bytes_received * 8.0) / (elapsed * 1000000.0);
    }
    
    printf("\r[RX] Progress: %.1fs | Packets: %lu | Bytes: %lu | Throughput: %.2f Mbps", 
           elapsed, stats.packets_received, stats.bytes_received, stats.throughput_mbps);
    
    if (rdma_cfg.enable_packet_loss) {
        double loss_rate = (double)(stats.packets_lost * 100) / 
                          (stats.packets_received + stats.packets_lost);
        printf(" | Loss: %.2f%%", loss_rate);
    }
    
    if (rdma_cfg.enable_latency_test && stats.packets_received > 0) {
        printf(" | Latency: %.2f/%.2f/%.2f μs", 
               stats.min_latency_us, stats.avg_latency_us, stats.max_latency_us);
    }
    
    if (rdma_cfg.enable_pattern_verify && stats.packets_corrupted > 0) {
        printf(" | Corrupted: %lu", stats.packets_corrupted);
    }
    
    fflush(stdout);
}

void save_test_results() {
    if (strlen(rdma_cfg.output_file) == 0) return;
    
    FILE *f = fopen(rdma_cfg.output_file, "w");
    if (!f) {
        LOG("[RX] Failed to open output file: %s", rdma_cfg.output_file);
        return;
    }
    
    double total_time = calculate_elapsed_time(&stats.start_time, &stats.end_time);
    
    fprintf(f, "# RDMA Receiver Test Results\n");
    fprintf(f, "Provider: %s\n", rdma_cfg.rdma_provider);
    fprintf(f, "Endpoints: %d\n", rdma_cfg.num_endpoints);
    fprintf(f, "Buffer Queue Capacity: %d\n", rdma_cfg.buffer_queue_capacity);
    fprintf(f, "Payload Type: %s\n", rdma_cfg.payload_type);
    fprintf(f, "Test Pattern: %s\n", rdma_cfg.test_pattern);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Packets Received: %lu\n", stats.packets_received);
    fprintf(f, "Bytes Received: %lu\n", stats.bytes_received);
    
    if (rdma_cfg.enable_packet_loss) {
        fprintf(f, "Packets Lost: %lu\n", stats.packets_lost);
        double loss_rate = (double)(stats.packets_lost * 100) / 
                          (stats.packets_received + stats.packets_lost);
        fprintf(f, "Packet Loss Rate: %.2f%%\n", loss_rate);
    }
    
    if (rdma_cfg.enable_pattern_verify) {
        fprintf(f, "Packets Corrupted: %lu\n", stats.packets_corrupted);
        double corruption_rate = (double)(stats.packets_corrupted * 100) / stats.packets_received;
        fprintf(f, "Corruption Rate: %.2f%%\n", corruption_rate);
    }
    
    if (rdma_cfg.enable_throughput_test) {
        fprintf(f, "Average Throughput: %.2f Mbps\n", stats.throughput_mbps);
    }
    
    if (rdma_cfg.enable_latency_test) {
        fprintf(f, "Min Latency: %.2f μs\n", stats.min_latency_us);
        fprintf(f, "Average Latency: %.2f μs\n", stats.avg_latency_us);
        fprintf(f, "Max Latency: %.2f μs\n", stats.max_latency_us);
    }
    
    fclose(f);
    LOG("[RX] Test results saved to: %s", rdma_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[RX] Starting RDMA Configuration Receiver Test");
    LOG("[RX] Provider: %s, Endpoints: %d, Queue Capacity: %d", 
        rdma_cfg.rdma_provider, rdma_cfg.num_endpoints, rdma_cfg.buffer_queue_capacity);
    LOG("[RX] Payload: %s, Pattern: %s, Timeout: %d ms", 
        rdma_cfg.payload_type, rdma_cfg.test_pattern, rdma_cfg.timeout_ms);
    LOG("[RX] Test Duration: %d seconds", rdma_cfg.test_duration_sec);

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate RDMA connection configuration
    conn_cfg = generate_rdma_config();
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

    // Initialize test statistics
    gettimeofday(&stats.start_time, NULL);
    stats.expected_packet_num = 0;
    
    LOG("[RX] Starting RDMA performance test...");
    
    FILE *dump_file = NULL;
    if (strlen(rdma_cfg.dump_file) > 0) {
        dump_file = fopen(rdma_cfg.dump_file, "wb");
        if (!dump_file) {
            LOG("[RX] Warning: Failed to open dump file: %s", rdma_cfg.dump_file);
        }
    }
    
    // Main receive loop
    struct timeval test_end;
    gettimeofday(&test_end, NULL);
    test_end.tv_sec += rdma_cfg.test_duration_sec;
    
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
        err = mesh_get_buffer_timeout(connection, &buf, rdma_cfg.timeout_ms);
        
        if (err == MESH_ERR_CONN_CLOSED) {
            LOG("[RX] Connection closed");
            break;
        }
        
        if (err == MESH_ERR_TIMEOUT) {
            // Timeout - check for packet loss
            if (rdma_cfg.enable_packet_loss) {
                stats.packets_lost++;
                stats.expected_packet_num++;
            }
            continue;
        }
        
        if (err) {
            LOG("[RX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
            continue;
        }
        
        gettimeofday(&stats.last_packet_time, NULL);
        stats.packets_received++;
        stats.bytes_received += buf->payload_len;
        
        // Verify pattern if enabled
        if (rdma_cfg.enable_pattern_verify && buf->payload_len > sizeof(struct timeval)) {
            if (!verify_test_pattern((uint8_t*)buf->payload_ptr, buf->payload_len, 
                                   rdma_cfg.test_pattern, stats.expected_packet_num)) {
                stats.packets_corrupted++;
            }
        }
        
        // Update latency stats if enabled
        if (rdma_cfg.enable_latency_test && buf->payload_len >= sizeof(struct timeval)) {
            struct timeval *sent_time = (struct timeval *)buf->payload_ptr;
            update_latency_stats(sent_time);
        }
        
        // Dump data if requested
        if (dump_file) {
            fwrite(buf->payload_ptr, 1, buf->payload_len, dump_file);
        }
        
        // Update expected packet number for loss detection
        if (rdma_cfg.enable_packet_loss) {
            stats.expected_packet_num++;
        }
        
        err = mesh_put_buffer(&buf);
        if (err) {
            LOG("[RX] Failed to put buffer: %s (%d)", mesh_err2str(err), err);
        }
        
        // Print progress every 100 packets
        if (stats.packets_received % 100 == 0) {
            print_progress_stats();
        }
    }
    
    if (dump_file) {
        fclose(dump_file);
        LOG("[RX] Data dumped to: %s", rdma_cfg.dump_file);
    }
    
    gettimeofday(&stats.end_time, NULL);
    printf("\n");
    
    // Final statistics
    double total_time = calculate_elapsed_time(&stats.start_time, &stats.end_time);
    if (rdma_cfg.enable_throughput_test) {
        stats.throughput_mbps = (stats.bytes_received * 8.0) / (total_time * 1000000.0);
    }
    
    LOG("[RX] Test completed in %.2f seconds", total_time);
    LOG("[RX] Packets received: %lu", stats.packets_received);
    LOG("[RX] Total bytes received: %lu", stats.bytes_received);
    
    if (rdma_cfg.enable_packet_loss) {
        double loss_rate = (stats.packets_received + stats.packets_lost > 0) ?
                          (double)(stats.packets_lost * 100) / (stats.packets_received + stats.packets_lost) : 0.0;
        LOG("[RX] Packets lost: %lu (%.2f%% loss rate)", stats.packets_lost, loss_rate);
    }
    
    if (rdma_cfg.enable_pattern_verify) {
        double corruption_rate = (stats.packets_received > 0) ?
                               (double)(stats.packets_corrupted * 100) / stats.packets_received : 0.0;
        LOG("[RX] Packets corrupted: %lu (%.2f%% corruption rate)", stats.packets_corrupted, corruption_rate);
    }
    
    if (rdma_cfg.enable_throughput_test) {
        LOG("[RX] Average throughput: %.2f Mbps", stats.throughput_mbps);
    }
    
    if (rdma_cfg.enable_latency_test) {
        LOG("[RX] Latency - Min: %.2f μs, Avg: %.2f μs, Max: %.2f μs", 
            stats.min_latency_us, stats.avg_latency_us, stats.max_latency_us);
    }
    
    // Save results to file if specified
    save_test_results();

safe_exit:
    LOG("[RX] Shutting down connection");
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
