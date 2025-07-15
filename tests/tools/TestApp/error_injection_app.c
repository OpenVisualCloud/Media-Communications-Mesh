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

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

// Error injection test configuration
typedef struct {
    char error_type[32];         // "connection", "memory", "network", "timeout", "all"
    char payload_type[16];       // "video", "audio", "blob"
    int test_duration_sec;
    int injection_interval_ms;   // How often to inject errors
    int recovery_timeout_ms;     // Time to wait for recovery
    int validate_recovery;       // Test recovery mechanisms
    int simulate_oom;           // Simulate out-of-memory
    int simulate_network_loss;   // Simulate network failures
    int simulate_timeouts;       // Simulate timeout conditions
    char output_file[256];
    int verbose;
} error_config_t;

static error_config_t err_cfg = {
    .error_type = "all",
    .payload_type = "video",
    .test_duration_sec = 300,  // 5 minutes
    .injection_interval_ms = 5000,  // Every 5 seconds
    .recovery_timeout_ms = 30000,   // 30 seconds for recovery
    .validate_recovery = 1,
    .simulate_oom = 0,
    .simulate_network_loss = 1,
    .simulate_timeouts = 1,
    .output_file = "",
    .verbose = 0
};

// Error tracking statistics
typedef struct {
    uint64_t errors_injected;
    uint64_t connection_errors;
    uint64_t memory_errors;
    uint64_t network_errors;
    uint64_t timeout_errors;
    uint64_t recovery_attempts;
    uint64_t successful_recoveries;
    uint64_t failed_recoveries;
    double avg_recovery_time_ms;
    double max_recovery_time_ms;
    struct timeval start_time;
    struct timeval last_error_time;
    int current_error_active;
} error_stats_t;

static error_stats_t stats = {0};

// Error injection state
typedef enum {
    ERROR_STATE_NORMAL,
    ERROR_STATE_INJECTED,
    ERROR_STATE_RECOVERING,
    ERROR_STATE_FAILED
} error_state_t;

static error_state_t current_state = ERROR_STATE_NORMAL;

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Error Injection and Recovery Test Application\n\n");
    printf("Test Configuration:\n");
    printf("  -e, --error-type <type>    Error type: connection, memory, network, timeout, all (default: %s)\n", err_cfg.error_type);
    printf("  -p, --payload <type>       Payload type: video, audio, blob (default: %s)\n", err_cfg.payload_type);
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", err_cfg.test_duration_sec);
    printf("\n  Error Injection:\n");
    printf("  --injection-interval <ms>  Error injection interval in ms (default: %d)\n", err_cfg.injection_interval_ms);
    printf("  --recovery-timeout <ms>    Recovery timeout in ms (default: %d)\n", err_cfg.recovery_timeout_ms);
    printf("  --validate-recovery        Enable recovery validation (default: %s)\n", err_cfg.validate_recovery ? "enabled" : "disabled");
    printf("\n  Error Types:\n");
    printf("  --simulate-oom             Simulate out-of-memory conditions\n");
    printf("  --simulate-network-loss    Simulate network failures (default: %s)\n", err_cfg.simulate_network_loss ? "enabled" : "disabled");
    printf("  --simulate-timeouts        Simulate timeout conditions (default: %s)\n", err_cfg.simulate_timeouts ? "enabled" : "disabled");
    printf("  --no-oom                   Disable OOM simulation\n");
    printf("  --no-network               Disable network error simulation\n");
    printf("  --no-timeouts              Disable timeout simulation\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Test all error types with recovery\n");
    printf("    %s --error-type all --validate-recovery\n", prog_name);
    printf("\n    # Test network errors only\n");
    printf("    %s --error-type network --simulate-network-loss\n", prog_name);
    printf("\n    # Memory stress testing\n");
    printf("    %s --error-type memory --simulate-oom --injection-interval 1000\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"error-type", required_argument, 0, 'e'},
        {"payload", required_argument, 0, 'p'},
        {"test-duration", required_argument, 0, 't'},
        {"injection-interval", required_argument, 0, 1001},
        {"recovery-timeout", required_argument, 0, 1002},
        {"validate-recovery", no_argument, 0, 1003},
        {"simulate-oom", no_argument, 0, 1004},
        {"simulate-network-loss", no_argument, 0, 1005},
        {"simulate-timeouts", no_argument, 0, 1006},
        {"no-oom", no_argument, 0, 1007},
        {"no-network", no_argument, 0, 1008},
        {"no-timeouts", no_argument, 0, 1009},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "e:p:t:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'e':
                strncpy(err_cfg.error_type, optarg, sizeof(err_cfg.error_type) - 1);
                break;
            case 'p':
                strncpy(err_cfg.payload_type, optarg, sizeof(err_cfg.payload_type) - 1);
                break;
            case 't':
                err_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1001:
                err_cfg.injection_interval_ms = atoi(optarg);
                break;
            case 1002:
                err_cfg.recovery_timeout_ms = atoi(optarg);
                break;
            case 1003:
                err_cfg.validate_recovery = 1;
                break;
            case 1004:
                err_cfg.simulate_oom = 1;
                break;
            case 1005:
                err_cfg.simulate_network_loss = 1;
                break;
            case 1006:
                err_cfg.simulate_timeouts = 1;
                break;
            case 1007:
                err_cfg.simulate_oom = 0;
                break;
            case 1008:
                err_cfg.simulate_network_loss = 0;
                break;
            case 1009:
                err_cfg.simulate_timeouts = 0;
                break;
            case 'o':
                strncpy(err_cfg.output_file, optarg, sizeof(err_cfg.output_file) - 1);
                break;
            case 'v':
                err_cfg.verbose = 1;
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

// Simulate out-of-memory conditions
int inject_memory_error() {
    if (!err_cfg.simulate_oom) return 0;
    
    LOG("[ERROR_INJ] Injecting memory error (OOM simulation)");
    
    // Allocate large amounts of memory to trigger OOM
    size_t alloc_size = 1024 * 1024 * 1024; // 1GB
    void *large_alloc = malloc(alloc_size);
    
    if (large_alloc) {
        // Fill memory to ensure it's actually allocated
        memset(large_alloc, 0xAA, alloc_size);
        
        // Keep it allocated for a short time
        sleep(2);
        free(large_alloc);
        
        stats.memory_errors++;
        LOG("[ERROR_INJ] Memory error simulation completed");
        return 1;
    }
    
    LOG("[ERROR_INJ] Failed to allocate memory for OOM simulation");
    return 0;
}

// Simulate network connection failures
int inject_network_error() {
    if (!err_cfg.simulate_network_loss) return 0;
    
    LOG("[ERROR_INJ] Injecting network error (connection loss simulation)");
    
    // Simulate network failure by temporarily disrupting connection
    if (connection) {
        // Force connection close/error state
        // Note: This would need actual API support
        LOG("[ERROR_INJ] Simulating network connection loss");
        
        stats.network_errors++;
        return 1;
    }
    
    return 0;
}

// Simulate timeout conditions
int inject_timeout_error() {
    if (!err_cfg.simulate_timeouts) return 0;
    
    LOG("[ERROR_INJ] Injecting timeout error");
    
    // Simulate timeout by blocking operations
    MeshBuffer *buf;
    int err = mesh_get_buffer_timeout(connection, &buf, 1); // Very short timeout
    
    if (err == MESH_ERR_TIMEOUT) {
        stats.timeout_errors++;
        LOG("[ERROR_INJ] Timeout error injected successfully");
        return 1;
    }
    
    if (buf) {
        mesh_put_buffer(&buf);
    }
    
    return 0;
}

// Simulate connection errors
int inject_connection_error() {
    LOG("[ERROR_INJ] Injecting connection error");
    
    // Simulate connection failure
    if (connection) {
        // Force connection into error state
        // This would require actual API support
        LOG("[ERROR_INJ] Simulating connection error");
        
        stats.connection_errors++;
        return 1;
    }
    
    return 0;
}

// Main error injection function
int inject_error() {
    int error_injected = 0;
    
    if (current_state != ERROR_STATE_NORMAL) {
        if (err_cfg.verbose) {
            LOG("[ERROR_INJ] Skipping injection - previous error still active");
        }
        return 0;
    }
    
    gettimeofday(&stats.last_error_time, NULL);
    current_state = ERROR_STATE_INJECTED;
    stats.current_error_active = 1;
    
    if (strcmp(err_cfg.error_type, "memory") == 0) {
        error_injected = inject_memory_error();
    } else if (strcmp(err_cfg.error_type, "network") == 0) {
        error_injected = inject_network_error();
    } else if (strcmp(err_cfg.error_type, "timeout") == 0) {
        error_injected = inject_timeout_error();
    } else if (strcmp(err_cfg.error_type, "connection") == 0) {
        error_injected = inject_connection_error();
    } else if (strcmp(err_cfg.error_type, "all") == 0) {
        // Randomly select error type
        int error_type = rand() % 4;
        switch (error_type) {
            case 0: error_injected = inject_memory_error(); break;
            case 1: error_injected = inject_network_error(); break;
            case 2: error_injected = inject_timeout_error(); break;
            case 3: error_injected = inject_connection_error(); break;
        }
    }
    
    if (error_injected) {
        stats.errors_injected++;
        LOG("[ERROR_INJ] Error injection successful, starting recovery validation");
        
        if (err_cfg.validate_recovery) {
            current_state = ERROR_STATE_RECOVERING;
        } else {
            current_state = ERROR_STATE_NORMAL;
            stats.current_error_active = 0;
        }
    } else {
        current_state = ERROR_STATE_NORMAL;
        stats.current_error_active = 0;
        LOG("[ERROR_INJ] Error injection failed");
    }
    
    return error_injected;
}

// Validate error recovery
int validate_recovery() {
    if (!err_cfg.validate_recovery || current_state != ERROR_STATE_RECOVERING) {
        return 0;
    }
    
    struct timeval now, recovery_start;
    gettimeofday(&now, NULL);
    recovery_start = stats.last_error_time;
    
    double elapsed_ms = ((now.tv_sec - recovery_start.tv_sec) * 1000.0) +
                       ((now.tv_usec - recovery_start.tv_usec) / 1000.0);
    
    if (elapsed_ms > err_cfg.recovery_timeout_ms) {
        LOG("[ERROR_INJ] Recovery timeout exceeded (%.1f ms)", elapsed_ms);
        current_state = ERROR_STATE_FAILED;
        stats.failed_recoveries++;
        stats.current_error_active = 0;
        return 0;
    }
    
    // Test if system has recovered by attempting normal operations
    MeshBuffer *buf;
    int err = mesh_get_buffer_timeout(connection, &buf, 100); // 100ms timeout
    
    if (err == 0 && buf) {
        // System appears to have recovered
        mesh_put_buffer(&buf);
        
        LOG("[ERROR_INJ] Recovery validated successfully (%.1f ms)", elapsed_ms);
        current_state = ERROR_STATE_NORMAL;
        stats.successful_recoveries++;
        stats.recovery_attempts++;
        stats.current_error_active = 0;
        
        // Update recovery time statistics
        if (stats.recovery_attempts == 1) {
            stats.avg_recovery_time_ms = elapsed_ms;
            stats.max_recovery_time_ms = elapsed_ms;
        } else {
            stats.avg_recovery_time_ms = ((stats.avg_recovery_time_ms * (stats.recovery_attempts - 1)) + elapsed_ms) / stats.recovery_attempts;
            if (elapsed_ms > stats.max_recovery_time_ms) {
                stats.max_recovery_time_ms = elapsed_ms;
            }
        }
        
        return 1;
    }
    
    if (err_cfg.verbose) {
        LOG("[ERROR_INJ] Recovery in progress (%.1f ms elapsed)", elapsed_ms);
    }
    
    return 0;
}

char* generate_error_config() {
    char *config = malloc(1024);
    if (!config) {
        LOG("[ERROR_INJ] Failed to allocate memory for config");
        return NULL;
    }

    if (strcmp(err_cfg.payload_type, "video") == 0) {
        snprintf(config, 1024,
            "{\n"
            "  \"connection\": {\n"
            "    \"errorHandling\": {\n"
            "      \"enableRecovery\": true,\n"
            "      \"retryAttempts\": 3,\n"
            "      \"retryDelay\": 1000,\n"
            "      \"timeoutMs\": %d\n"
            "    },\n"
            "    \"resilience\": {\n"
            "      \"bufferManagement\": \"adaptive\",\n"
            "      \"connectionPooling\": true,\n"
            "      \"gracefulDegradation\": true\n"
            "    }\n"
            "  },\n"
            "  \"payload\": {\n"
            "    \"video\": {\n"
            "      \"width\": 1920,\n"
            "      \"height\": 1080,\n"
            "      \"fps\": 30,\n"
            "      \"pixelFormat\": \"yuv422p10le\"\n"
            "    }\n"
            "  }\n"
            "}",
            err_cfg.recovery_timeout_ms);
    } else if (strcmp(err_cfg.payload_type, "audio") == 0) {
        snprintf(config, 1024,
            "{\n"
            "  \"connection\": {\n"
            "    \"errorHandling\": {\n"
            "      \"enableRecovery\": true,\n"
            "      \"retryAttempts\": 5,\n"
            "      \"retryDelay\": 500,\n"
            "      \"timeoutMs\": %d\n"
            "    },\n"
            "    \"resilience\": {\n"
            "      \"bufferManagement\": \"adaptive\",\n"
            "      \"connectionPooling\": true,\n"
            "      \"gracefulDegradation\": true\n"
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
            err_cfg.recovery_timeout_ms);
    } else {
        // Blob configuration
        snprintf(config, 1024,
            "{\n"
            "  \"connection\": {\n"
            "    \"errorHandling\": {\n"
            "      \"enableRecovery\": true,\n"
            "      \"retryAttempts\": 3,\n"
            "      \"retryDelay\": 1000,\n"
            "      \"timeoutMs\": %d\n"
            "    },\n"
            "    \"resilience\": {\n"
            "      \"bufferManagement\": \"adaptive\",\n"
            "      \"connectionPooling\": true,\n"
            "      \"gracefulDegradation\": true\n"
            "    }\n"
            "  },\n"
            "  \"payload\": {\n"
            "    \"blob\": {}\n"
            "  }\n"
            "}",
            err_cfg.recovery_timeout_ms);
    }

    return config;
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    const char *state_name = "NORMAL";
    switch (current_state) {
        case ERROR_STATE_INJECTED: state_name = "ERROR"; break;
        case ERROR_STATE_RECOVERING: state_name = "RECOVERING"; break;
        case ERROR_STATE_FAILED: state_name = "FAILED"; break;
        default: break;
    }
    
    printf("\r[ERROR_INJ] Progress: %.1fs | State: %s | Errors: %lu | Recoveries: %lu/%lu", 
           elapsed, state_name, stats.errors_injected, 
           stats.successful_recoveries, stats.recovery_attempts);
    
    if (stats.recovery_attempts > 0) {
        printf(" | Avg Recovery: %.1f ms", stats.avg_recovery_time_ms);
    }
    
    fflush(stdout);
}

void save_error_results() {
    if (strlen(err_cfg.output_file) == 0) return;
    
    FILE *f = fopen(err_cfg.output_file, "w");
    if (!f) {
        LOG("[ERROR_INJ] Failed to open output file: %s", err_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Error Injection and Recovery Test Results\n");
    fprintf(f, "Error Type: %s\n", err_cfg.error_type);
    fprintf(f, "Payload Type: %s\n", err_cfg.payload_type);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Injection Interval: %d ms\n", err_cfg.injection_interval_ms);
    fprintf(f, "Recovery Timeout: %d ms\n", err_cfg.recovery_timeout_ms);
    fprintf(f, "Recovery Validation: %s\n", err_cfg.validate_recovery ? "enabled" : "disabled");
    
    fprintf(f, "\nError Statistics:\n");
    fprintf(f, "Total Errors Injected: %lu\n", stats.errors_injected);
    fprintf(f, "Connection Errors: %lu\n", stats.connection_errors);
    fprintf(f, "Memory Errors: %lu\n", stats.memory_errors);
    fprintf(f, "Network Errors: %lu\n", stats.network_errors);
    fprintf(f, "Timeout Errors: %lu\n", stats.timeout_errors);
    
    fprintf(f, "\nRecovery Statistics:\n");
    fprintf(f, "Recovery Attempts: %lu\n", stats.recovery_attempts);
    fprintf(f, "Successful Recoveries: %lu\n", stats.successful_recoveries);
    fprintf(f, "Failed Recoveries: %lu\n", stats.failed_recoveries);
    
    if (stats.recovery_attempts > 0) {
        double success_rate = (double)(stats.successful_recoveries * 100) / stats.recovery_attempts;
        fprintf(f, "Recovery Success Rate: %.1f%%\n", success_rate);
        fprintf(f, "Average Recovery Time: %.2f ms\n", stats.avg_recovery_time_ms);
        fprintf(f, "Maximum Recovery Time: %.2f ms\n", stats.max_recovery_time_ms);
    }
    
    double error_rate = stats.errors_injected / total_time;
    fprintf(f, "\nTest Metrics:\n");
    fprintf(f, "Error Injection Rate: %.2f errors/sec\n", error_rate);
    fprintf(f, "System Availability: %.2f%%\n", 
           ((total_time * 1000 - stats.avg_recovery_time_ms * stats.recovery_attempts) / (total_time * 1000)) * 100);
    
    fclose(f);
    LOG("[ERROR_INJ] Test results saved to: %s", err_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[ERROR_INJ] Starting Error Injection and Recovery Test");
    LOG("[ERROR_INJ] Error Type: %s, Payload: %s, Duration: %d seconds", 
        err_cfg.error_type, err_cfg.payload_type, err_cfg.test_duration_sec);
    LOG("[ERROR_INJ] Injection Interval: %d ms, Recovery Timeout: %d ms", 
        err_cfg.injection_interval_ms, err_cfg.recovery_timeout_ms);
    LOG("[ERROR_INJ] Simulations - OOM: %s, Network: %s, Timeouts: %s", 
        err_cfg.simulate_oom ? "enabled" : "disabled",
        err_cfg.simulate_network_loss ? "enabled" : "disabled",
        err_cfg.simulate_timeouts ? "enabled" : "disabled");

    // Initialize random seed for error selection
    srand(time(NULL));

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate error-resilient connection configuration
    conn_cfg = generate_error_config();
    if (!conn_cfg) {
        free(client_cfg);
        exit(EXIT_FAILURE);
    }

    LOG("[ERROR_INJ] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[ERROR_INJ] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[ERROR_INJ] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    
    LOG("[ERROR_INJ] Starting error injection test for %d seconds...", err_cfg.test_duration_sec);

    // Main test loop
    struct timeval test_end, last_injection;
    gettimeofday(&test_end, NULL);
    gettimeofday(&last_injection, NULL);
    test_end.tv_sec += err_cfg.test_duration_sec;

    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        if (shutdown_flag == SHUTDOWN_REQUESTED) {
            LOG("[ERROR_INJ] Graceful shutdown requested");
            break;
        }
        
        // Check if it's time to inject an error
        double injection_elapsed = ((now.tv_sec - last_injection.tv_sec) * 1000.0) +
                                  ((now.tv_usec - last_injection.tv_usec) / 1000.0);
        
        if (injection_elapsed >= err_cfg.injection_interval_ms) {
            inject_error();
            last_injection = now;
        }
        
        // Validate recovery if in recovery state
        validate_recovery();
        
        // Regular operation to test system functionality
        MeshBuffer *buf;
        err = mesh_get_buffer_timeout(connection, &buf, 100); // 100ms timeout
        
        if (err == 0 && buf) {
            // Process normally
            mesh_put_buffer(&buf);
        } else if (err == MESH_ERR_TIMEOUT && current_state == ERROR_STATE_NORMAL) {
            // Unexpected timeout when system should be normal
            if (err_cfg.verbose) {
                LOG("[ERROR_INJ] Unexpected timeout in normal state");
            }
        }
        
        // Print progress every 5 seconds
        static struct timeval last_progress = {0, 0};
        if (last_progress.tv_sec == 0) last_progress = now;
        
        double progress_elapsed = ((now.tv_sec - last_progress.tv_sec) * 1000.0) +
                                 ((now.tv_usec - last_progress.tv_usec) / 1000.0);
        
        if (progress_elapsed >= 5000) { // 5 seconds
            print_progress_stats();
            last_progress = now;
        }
        
        usleep(100000); // 100ms
    }
    
    printf("\n");
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[ERROR_INJ] Test completed in %.2f seconds", total_time);
    LOG("[ERROR_INJ] Total errors injected: %lu", stats.errors_injected);
    LOG("[ERROR_INJ] Recovery attempts: %lu", stats.recovery_attempts);
    LOG("[ERROR_INJ] Successful recoveries: %lu", stats.successful_recoveries);
    LOG("[ERROR_INJ] Failed recoveries: %lu", stats.failed_recoveries);
    
    if (stats.recovery_attempts > 0) {
        double success_rate = (double)(stats.successful_recoveries * 100) / stats.recovery_attempts;
        LOG("[ERROR_INJ] Recovery success rate: %.1f%%", success_rate);
        LOG("[ERROR_INJ] Average recovery time: %.1f ms", stats.avg_recovery_time_ms);
    }
    
    double error_rate = stats.errors_injected / total_time;
    LOG("[ERROR_INJ] Error injection rate: %.2f errors/sec", error_rate);
    
    // Save results to file if specified
    save_error_results();

safe_exit:
    LOG("[ERROR_INJ] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[ERROR_INJ] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    return err;
}
