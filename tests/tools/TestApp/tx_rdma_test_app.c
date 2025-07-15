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
    int packet_size;            // Packet size for testing
    int burst_size;             // Number of packets per burst
    int inter_burst_delay_us;   // Delay between bursts (microseconds)
    char test_pattern[32];       // "sequential", "random", "zero"
    int enable_latency_test;     // Enable latency measurements
    int enable_throughput_test;  // Enable throughput measurements
    int enable_cpu_usage;        // Monitor CPU usage
    char output_file[256];       // Output file for test results
} rdma_test_config_t;

static rdma_test_config_t rdma_cfg = {
    .rdma_provider = "tcp",
    .num_endpoints = 1,
    .buffer_queue_capacity = 16,
    .conn_delay_ms = 0,
    .payload_type = "blob",
    .test_duration_sec = 30,
    .packet_size = 1024,
    .burst_size = 10,
    .inter_burst_delay_us = 1000,
    .test_pattern = "sequential",
    .enable_latency_test = 1,
    .enable_throughput_test = 1,
    .enable_cpu_usage = 0,
    .output_file = ""
};

// Test statistics
typedef struct {
    uint64_t packets_sent;
    uint64_t bytes_sent;
    uint64_t packets_failed;
    double min_latency_us;
    double max_latency_us;
    double avg_latency_us;
    double throughput_mbps;
    struct timeval start_time;
    struct timeval end_time;
    double cpu_usage_percent;
} test_stats_t;

static test_stats_t stats = {0};

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n");
    printf("RDMA Configuration Transmitter Test Application\n\n");
    printf("Options:\n");
    printf("  --rdma-provider <prov>     RDMA provider: tcp, verbs (default: %s)\n", rdma_cfg.rdma_provider);
    printf("  --rdma-endpoints <num>     Number of RDMA endpoints 1-8 (default: %d)\n", rdma_cfg.num_endpoints);
    printf("  -q, --queue-capacity <num> Buffer queue capacity (default: %d)\n", rdma_cfg.buffer_queue_capacity);
    printf("  -d, --delay <ms>           Connection creation delay in ms (default: %d)\n", rdma_cfg.conn_delay_ms);
    printf("\n  Test Configuration:\n");
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", rdma_cfg.test_duration_sec);
    printf("  -s, --packet-size <bytes>  Packet size in bytes (default: %d)\n", rdma_cfg.packet_size);
    printf("  -b, --burst-size <count>   Number of packets per burst (default: %d)\n", rdma_cfg.burst_size);
    printf("  --burst-delay <us>         Delay between bursts in microseconds (default: %d)\n", rdma_cfg.inter_burst_delay_us);
    printf("  --pattern <type>           Test pattern: sequential, random, zero (default: %s)\n", rdma_cfg.test_pattern);
    printf("  --payload-type <type>      Payload type: video, audio, blob (default: %s)\n", rdma_cfg.payload_type);
    printf("\n  Performance Measurements:\n");
    printf("  --enable-latency           Enable latency measurements (default: %s)\n", rdma_cfg.enable_latency_test ? "enabled" : "disabled");
    printf("  --disable-latency          Disable latency measurements\n");
    printf("  --enable-throughput        Enable throughput measurements (default: %s)\n", rdma_cfg.enable_throughput_test ? "enabled" : "disabled");
    printf("  --disable-throughput       Disable throughput measurements\n");
    printf("  --enable-cpu               Enable CPU usage monitoring\n");
    printf("  --disable-cpu              Disable CPU usage monitoring (default)\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("\n  General:\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Basic TCP RDMA test\n");
    printf("    %s --rdma-provider tcp --rdma-endpoints 2\n", prog_name);
    printf("\n    # High-performance verbs test with latency measurement\n");
    printf("    %s --rdma-provider verbs --rdma-endpoints 8 --packet-size 8192 --enable-latency\n", prog_name);
    printf("\n    # Burst throughput test\n");
    printf("    %s --burst-size 100 --burst-delay 10000 --test-duration 60\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"rdma-provider", required_argument, 0, 1001},
        {"rdma-endpoints", required_argument, 0, 1002},
        {"queue-capacity", required_argument, 0, 'q'},
        {"delay", required_argument, 0, 'd'},
        {"test-duration", required_argument, 0, 't'},
        {"packet-size", required_argument, 0, 's'},
        {"burst-size", required_argument, 0, 'b'},
        {"burst-delay", required_argument, 0, 1003},
        {"pattern", required_argument, 0, 1004},
        {"payload-type", required_argument, 0, 1005},
        {"enable-latency", no_argument, 0, 1006},
        {"disable-latency", no_argument, 0, 1007},
        {"enable-throughput", no_argument, 0, 1008},
        {"disable-throughput", no_argument, 0, 1009},
        {"enable-cpu", no_argument, 0, 1010},
        {"disable-cpu", no_argument, 0, 1011},
        {"output", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "q:d:t:s:b:o:h", long_options, NULL)) != -1) {
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
            case 's':
                rdma_cfg.packet_size = atoi(optarg);
                break;
            case 'b':
                rdma_cfg.burst_size = atoi(optarg);
                break;
            case 1003:
                rdma_cfg.inter_burst_delay_us = atoi(optarg);
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
                rdma_cfg.enable_cpu_usage = 1;
                break;
            case 1011:
                rdma_cfg.enable_cpu_usage = 0;
                break;
            case 'o':
                strncpy(rdma_cfg.output_file, optarg, sizeof(rdma_cfg.output_file) - 1);
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
        LOG("[TX] Failed to allocate memory for config");
        return NULL;
    }

    if (strcmp(rdma_cfg.payload_type, "video") == 0) {
        snprintf(config, 1024,
            "{\n"
            "  \"bufferQueueCapacity\": %d,\n"
            "  \"connCreationDelayMilliseconds\": %d,\n"
            "  \"connection\": {\n"
            "    \"memif\": {\n"
            "      \"interface\": \"tx_memif\",\n"
            "      \"socketPath\": \"/run/mcm/mcm_tx_memif.sock\"\n"
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
            "      \"interface\": \"tx_memif\",\n"
            "      \"socketPath\": \"/run/mcm/mcm_tx_memif.sock\"\n"
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
            "      \"interface\": \"tx_memif\",\n"
            "      \"socketPath\": \"/run/mcm/mcm_tx_memif.sock\"\n"
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

void fill_test_pattern(uint8_t *buffer, size_t size, const char *pattern, uint64_t packet_num) {
    if (strcmp(pattern, "sequential") == 0) {
        for (size_t i = 0; i < size; i++) {
            buffer[i] = (uint8_t)((packet_num + i) & 0xFF);
        }
    } else if (strcmp(pattern, "random") == 0) {
        srand(packet_num);
        for (size_t i = 0; i < size; i++) {
            buffer[i] = (uint8_t)(rand() & 0xFF);
        }
    } else { // zero pattern
        memset(buffer, 0, size);
    }
    
    // Add timestamp for latency measurement if enabled
    if (rdma_cfg.enable_latency_test && size >= sizeof(struct timeval)) {
        struct timeval *ts = (struct timeval *)buffer;
        gettimeofday(ts, NULL);
    }
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
    
    if (stats.packets_sent == 1) {
        stats.min_latency_us = stats.max_latency_us = latency_us;
        stats.avg_latency_us = latency_us;
    } else {
        if (latency_us < stats.min_latency_us) stats.min_latency_us = latency_us;
        if (latency_us > stats.max_latency_us) stats.max_latency_us = latency_us;
        stats.avg_latency_us = ((stats.avg_latency_us * (stats.packets_sent - 1)) + latency_us) / stats.packets_sent;
    }
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = calculate_elapsed_time(&stats.start_time, &now);
    
    if (rdma_cfg.enable_throughput_test) {
        stats.throughput_mbps = (stats.bytes_sent * 8.0) / (elapsed * 1000000.0);
    }
    
    printf("\r[TX] Progress: %.1fs | Packets: %lu | Bytes: %lu | Throughput: %.2f Mbps", 
           elapsed, stats.packets_sent, stats.bytes_sent, stats.throughput_mbps);
    if (rdma_cfg.enable_latency_test && stats.packets_sent > 0) {
        printf(" | Latency: %.2f/%.2f/%.2f μs (min/avg/max)", 
               stats.min_latency_us, stats.avg_latency_us, stats.max_latency_us);
    }
    fflush(stdout);
}

void save_test_results() {
    if (strlen(rdma_cfg.output_file) == 0) return;
    
    FILE *f = fopen(rdma_cfg.output_file, "w");
    if (!f) {
        LOG("[TX] Failed to open output file: %s", rdma_cfg.output_file);
        return;
    }
    
    double total_time = calculate_elapsed_time(&stats.start_time, &stats.end_time);
    
    fprintf(f, "# RDMA Test Results\n");
    fprintf(f, "Provider: %s\n", rdma_cfg.rdma_provider);
    fprintf(f, "Endpoints: %d\n", rdma_cfg.num_endpoints);
    fprintf(f, "Buffer Queue Capacity: %d\n", rdma_cfg.buffer_queue_capacity);
    fprintf(f, "Payload Type: %s\n", rdma_cfg.payload_type);
    fprintf(f, "Packet Size: %d bytes\n", rdma_cfg.packet_size);
    fprintf(f, "Burst Size: %d packets\n", rdma_cfg.burst_size);
    fprintf(f, "Test Pattern: %s\n", rdma_cfg.test_pattern);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Packets Sent: %lu\n", stats.packets_sent);
    fprintf(f, "Packets Failed: %lu\n", stats.packets_failed);
    fprintf(f, "Bytes Sent: %lu\n", stats.bytes_sent);
    fprintf(f, "Success Rate: %.2f%%\n", 
            (double)(stats.packets_sent * 100) / (stats.packets_sent + stats.packets_failed));
    
    if (rdma_cfg.enable_throughput_test) {
        fprintf(f, "Average Throughput: %.2f Mbps\n", stats.throughput_mbps);
    }
    
    if (rdma_cfg.enable_latency_test) {
        fprintf(f, "Min Latency: %.2f μs\n", stats.min_latency_us);
        fprintf(f, "Average Latency: %.2f μs\n", stats.avg_latency_us);
        fprintf(f, "Max Latency: %.2f μs\n", stats.max_latency_us);
    }
    
    fclose(f);
    LOG("[TX] Test results saved to: %s", rdma_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[TX] Starting RDMA Configuration Test");
    LOG("[TX] Provider: %s, Endpoints: %d, Queue Capacity: %d", 
        rdma_cfg.rdma_provider, rdma_cfg.num_endpoints, rdma_cfg.buffer_queue_capacity);
    LOG("[TX] Payload: %s, Packet Size: %d, Burst Size: %d", 
        rdma_cfg.payload_type, rdma_cfg.packet_size, rdma_cfg.burst_size);
    LOG("[TX] Test Duration: %d seconds, Pattern: %s", 
        rdma_cfg.test_duration_sec, rdma_cfg.test_pattern);

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

    // Initialize test statistics
    gettimeofday(&stats.start_time, NULL);
    
    LOG("[TX] Starting RDMA performance test...");
    
    // Main test loop
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
            LOG("[TX] Graceful shutdown requested");
            break;
        }
        
        // Send burst of packets
        for (int burst = 0; burst < rdma_cfg.burst_size; burst++) {
            MeshBuffer *buf;
            
            err = mesh_get_buffer(connection, &buf);
            if (err) {
                LOG("[TX] Failed to get buffer: %s (%d)", mesh_err2str(err), err);
                stats.packets_failed++;
                continue;
            }
            
            size_t packet_size = (buf->payload_len < rdma_cfg.packet_size) ? 
                               buf->payload_len : rdma_cfg.packet_size;
            
            fill_test_pattern((uint8_t*)buf->payload_ptr, packet_size, 
                            rdma_cfg.test_pattern, stats.packets_sent);
            
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
                continue;
            }
            
            stats.packets_sent++;
            stats.bytes_sent += packet_size;
            
            // Update latency stats if enabled
            if (rdma_cfg.enable_latency_test && packet_size >= sizeof(struct timeval)) {
                struct timeval *sent_time = (struct timeval *)buf->payload_ptr;
                update_latency_stats(sent_time);
            }
        }
        
        // Inter-burst delay
        if (rdma_cfg.inter_burst_delay_us > 0) {
            usleep(rdma_cfg.inter_burst_delay_us);
        }
        
        // Print progress every second
        if (stats.packets_sent % 100 == 0) {
            print_progress_stats();
        }
    }
    
    gettimeofday(&stats.end_time, NULL);
    printf("\n");
    
    // Final statistics
    double total_time = calculate_elapsed_time(&stats.start_time, &stats.end_time);
    if (rdma_cfg.enable_throughput_test) {
        stats.throughput_mbps = (stats.bytes_sent * 8.0) / (total_time * 1000000.0);
    }
    
    LOG("[TX] Test completed in %.2f seconds", total_time);
    LOG("[TX] Packets sent: %lu, Failed: %lu (%.2f%% success rate)", 
        stats.packets_sent, stats.packets_failed,
        (double)(stats.packets_sent * 100) / (stats.packets_sent + stats.packets_failed));
    LOG("[TX] Total bytes sent: %lu", stats.bytes_sent);
    
    if (rdma_cfg.enable_throughput_test) {
        LOG("[TX] Average throughput: %.2f Mbps", stats.throughput_mbps);
    }
    
    if (rdma_cfg.enable_latency_test) {
        LOG("[TX] Latency - Min: %.2f μs, Avg: %.2f μs, Max: %.2f μs", 
            stats.min_latency_us, stats.avg_latency_us, stats.max_latency_us);
    }
    
    // Save results to file if specified
    save_test_results();

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
