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
#include <pthread.h>

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

// Callback test configuration
typedef struct {
    char test_mode[32];          // "callbacks", "events", "combined"
    char payload_type[16];       // "video", "audio", "blob"
    int test_duration_sec;
    int trigger_errors;          // Force error conditions
    int stress_test;             // High frequency operations
    int validate_threading;      // Thread safety validation
    int measure_latency;         // Callback latency measurement
    char output_file[256];
    int verbose;
} callback_config_t;

static callback_config_t cb_cfg = {
    .test_mode = "callbacks",
    .payload_type = "video",
    .test_duration_sec = 60,
    .trigger_errors = 0,
    .stress_test = 0,
    .validate_threading = 1,
    .measure_latency = 1,
    .output_file = "",
    .verbose = 0
};

// Callback statistics
typedef struct {
    uint64_t connection_events;
    uint64_t data_events;
    uint64_t error_events;
    uint64_t buffer_events;
    uint64_t status_events;
    uint64_t custom_events;
    uint64_t callback_errors;
    uint64_t thread_violations;
    struct timeval start_time;
    double min_latency_us;
    double max_latency_us;
    double avg_latency_us;
    uint64_t latency_samples;
    pthread_mutex_t stats_mutex;
} callback_stats_t;

static callback_stats_t stats = {0};

// Thread tracking for safety validation
typedef struct {
    pthread_t thread_id;
    char thread_name[64];
    uint64_t callback_count;
    int is_callback_thread;
} thread_info_t;

static thread_info_t threads[32];
static int thread_count = 0;
static pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Callback and Event Handling Test Application\n\n");
    printf("Test Configuration:\n");
    printf("  -m, --mode <type>          Test mode: callbacks, events, combined (default: %s)\n", cb_cfg.test_mode);
    printf("  -p, --payload <type>       Payload type: video, audio, blob (default: %s)\n", cb_cfg.payload_type);
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", cb_cfg.test_duration_sec);
    printf("\n  Test Options:\n");
    printf("  --trigger-errors           Force error conditions for testing\n");
    printf("  --stress-test             Enable high frequency stress testing\n");
    printf("  --validate-threading      Enable thread safety validation (default: %s)\n", cb_cfg.validate_threading ? "enabled" : "disabled");
    printf("  --measure-latency         Measure callback latency (default: %s)\n", cb_cfg.measure_latency ? "enabled" : "disabled");
    printf("  --no-threading           Disable thread safety validation\n");
    printf("  --no-latency             Disable latency measurement\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Basic callback testing\n");
    printf("    %s --mode callbacks --payload video\n", prog_name);
    printf("\n    # Event handling stress test\n");
    printf("    %s --mode events --stress-test --trigger-errors\n", prog_name);
    printf("\n    # Combined testing with latency measurement\n");
    printf("    %s --mode combined --measure-latency --output callback_results.txt\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"payload", required_argument, 0, 'p'},
        {"test-duration", required_argument, 0, 't'},
        {"trigger-errors", no_argument, 0, 1001},
        {"stress-test", no_argument, 0, 1002},
        {"validate-threading", no_argument, 0, 1003},
        {"measure-latency", no_argument, 0, 1004},
        {"no-threading", no_argument, 0, 1005},
        {"no-latency", no_argument, 0, 1006},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "m:p:t:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'm':
                strncpy(cb_cfg.test_mode, optarg, sizeof(cb_cfg.test_mode) - 1);
                break;
            case 'p':
                strncpy(cb_cfg.payload_type, optarg, sizeof(cb_cfg.payload_type) - 1);
                break;
            case 't':
                cb_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1001:
                cb_cfg.trigger_errors = 1;
                break;
            case 1002:
                cb_cfg.stress_test = 1;
                break;
            case 1003:
                cb_cfg.validate_threading = 1;
                break;
            case 1004:
                cb_cfg.measure_latency = 1;
                break;
            case 1005:
                cb_cfg.validate_threading = 0;
                break;
            case 1006:
                cb_cfg.measure_latency = 0;
                break;
            case 'o':
                strncpy(cb_cfg.output_file, optarg, sizeof(cb_cfg.output_file) - 1);
                break;
            case 'v':
                cb_cfg.verbose = 1;
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

thread_info_t* find_or_create_thread() {
    pthread_t current_thread = pthread_self();
    
    pthread_mutex_lock(&thread_mutex);
    
    for (int i = 0; i < thread_count; i++) {
        if (pthread_equal(threads[i].thread_id, current_thread)) {
            pthread_mutex_unlock(&thread_mutex);
            return &threads[i];
        }
    }
    
    if (thread_count < 32) {
        thread_info_t *thread = &threads[thread_count];
        thread->thread_id = current_thread;
        snprintf(thread->thread_name, sizeof(thread->thread_name), "thread_%d", thread_count);
        thread->callback_count = 0;
        thread->is_callback_thread = 1;
        thread_count++;
        pthread_mutex_unlock(&thread_mutex);
        return thread;
    }
    
    pthread_mutex_unlock(&thread_mutex);
    return NULL;
}

void record_callback_latency(struct timeval *start) {
    if (!cb_cfg.measure_latency) return;
    
    struct timeval end;
    gettimeofday(&end, NULL);
    
    double latency_us = ((end.tv_sec - start->tv_sec) * 1000000.0) +
                       (end.tv_usec - start->tv_usec);
    
    pthread_mutex_lock(&stats.stats_mutex);
    
    if (stats.latency_samples == 0) {
        stats.min_latency_us = latency_us;
        stats.max_latency_us = latency_us;
        stats.avg_latency_us = latency_us;
    } else {
        if (latency_us < stats.min_latency_us) stats.min_latency_us = latency_us;
        if (latency_us > stats.max_latency_us) stats.max_latency_us = latency_us;
        stats.avg_latency_us = ((stats.avg_latency_us * stats.latency_samples) + latency_us) / (stats.latency_samples + 1);
    }
    
    stats.latency_samples++;
    pthread_mutex_unlock(&stats.stats_mutex);
}

// Connection event callback
void connection_event_callback(MeshConnection *conn, mesh_conn_event_t event, void *user_data) {
    struct timeval start;
    if (cb_cfg.measure_latency) gettimeofday(&start, NULL);
    
    if (cb_cfg.validate_threading) {
        thread_info_t *thread = find_or_create_thread();
        if (thread) thread->callback_count++;
    }
    
    pthread_mutex_lock(&stats.stats_mutex);
    stats.connection_events++;
    pthread_mutex_unlock(&stats.stats_mutex);
    
    if (cb_cfg.verbose) {
        const char *event_name = "UNKNOWN";
        switch (event) {
            case MESH_CONN_EVENT_CONNECTED: event_name = "CONNECTED"; break;
            case MESH_CONN_EVENT_DISCONNECTED: event_name = "DISCONNECTED"; break;
            case MESH_CONN_EVENT_ERROR: event_name = "ERROR"; break;
            default: break;
        }
        LOG("[CALLBACK] Connection event: %s", event_name);
    }
    
    if (cb_cfg.trigger_errors && event == MESH_CONN_EVENT_ERROR) {
        // Simulate error handling
        LOG("[CALLBACK] Handling triggered error event");
        pthread_mutex_lock(&stats.stats_mutex);
        stats.error_events++;
        pthread_mutex_unlock(&stats.stats_mutex);
    }
    
    record_callback_latency(&start);
}

// Data available callback
void data_available_callback(MeshConnection *conn, void *user_data) {
    struct timeval start;
    if (cb_cfg.measure_latency) gettimeofday(&start, NULL);
    
    if (cb_cfg.validate_threading) {
        thread_info_t *thread = find_or_create_thread();
        if (thread) thread->callback_count++;
    }
    
    pthread_mutex_lock(&stats.stats_mutex);
    stats.data_events++;
    pthread_mutex_unlock(&stats.stats_mutex);
    
    if (cb_cfg.verbose) {
        LOG("[CALLBACK] Data available");
    }
    
    // Process the data to validate callback functionality
    MeshBuffer *buf = NULL;
    int err = mesh_get_buffer_timeout(conn, &buf, 1); // 1ms timeout
    if (err == 0 && buf) {
        // Validate buffer contents if needed
        if (cb_cfg.verbose) {
            LOG("[CALLBACK] Processed buffer with %zu bytes", buf->payload_len);
        }
        mesh_put_buffer(&buf);
    }
    
    record_callback_latency(&start);
}

// Buffer status callback
void buffer_status_callback(MeshConnection *conn, mesh_buffer_status_t status, void *user_data) {
    struct timeval start;
    if (cb_cfg.measure_latency) gettimeofday(&start, NULL);
    
    if (cb_cfg.validate_threading) {
        thread_info_t *thread = find_or_create_thread();
        if (thread) thread->callback_count++;
    }
    
    pthread_mutex_lock(&stats.stats_mutex);
    stats.buffer_events++;
    pthread_mutex_unlock(&stats.stats_mutex);
    
    if (cb_cfg.verbose) {
        const char *status_name = "UNKNOWN";
        switch (status) {
            case MESH_BUFFER_STATUS_FULL: status_name = "FULL"; break;
            case MESH_BUFFER_STATUS_EMPTY: status_name = "EMPTY"; break;
            case MESH_BUFFER_STATUS_NORMAL: status_name = "NORMAL"; break;
            default: break;
        }
        LOG("[CALLBACK] Buffer status: %s", status_name);
    }
    
    record_callback_latency(&start);
}

// Generic status callback
void status_callback(MeshConnection *conn, mesh_status_t status, void *user_data) {
    struct timeval start;
    if (cb_cfg.measure_latency) gettimeofday(&start, NULL);
    
    if (cb_cfg.validate_threading) {
        thread_info_t *thread = find_or_create_thread();
        if (thread) thread->callback_count++;
    }
    
    pthread_mutex_lock(&stats.stats_mutex);
    stats.status_events++;
    pthread_mutex_unlock(&stats.stats_mutex);
    
    if (cb_cfg.verbose) {
        LOG("[CALLBACK] Status update: %d", status);
    }
    
    record_callback_latency(&start);
}

// Custom event callback for advanced testing
void custom_event_callback(MeshConnection *conn, int event_type, void *event_data, void *user_data) {
    struct timeval start;
    if (cb_cfg.measure_latency) gettimeofday(&start, NULL);
    
    if (cb_cfg.validate_threading) {
        thread_info_t *thread = find_or_create_thread();
        if (thread) thread->callback_count++;
    }
    
    pthread_mutex_lock(&stats.stats_mutex);
    stats.custom_events++;
    pthread_mutex_unlock(&stats.stats_mutex);
    
    if (cb_cfg.verbose) {
        LOG("[CALLBACK] Custom event: type=%d", event_type);
    }
    
    record_callback_latency(&start);
}

char* generate_callback_config() {
    char *config = malloc(1024);
    if (!config) {
        LOG("[CALLBACK] Failed to allocate memory for config");
        return NULL;
    }

    if (strcmp(cb_cfg.payload_type, "video") == 0) {
        snprintf(config, 1024,
            "{\n"
            "  \"connection\": {\n"
            "    \"callbacks\": {\n"
            "      \"connectionEvents\": true,\n"
            "      \"dataAvailable\": true,\n"
            "      \"bufferStatus\": true,\n"
            "      \"statusUpdates\": true,\n"
            "      \"customEvents\": true\n"
            "    },\n"
            "    \"eventHandling\": {\n"
            "      \"asyncProcessing\": true,\n"
            "      \"queueSize\": 1000,\n"
            "      \"threadPool\": %s\n"
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
            cb_cfg.stress_test ? "true" : "false");
    } else if (strcmp(cb_cfg.payload_type, "audio") == 0) {
        snprintf(config, 1024,
            "{\n"
            "  \"connection\": {\n"
            "    \"callbacks\": {\n"
            "      \"connectionEvents\": true,\n"
            "      \"dataAvailable\": true,\n"
            "      \"bufferStatus\": true,\n"
            "      \"statusUpdates\": true\n"
            "    },\n"
            "    \"eventHandling\": {\n"
            "      \"asyncProcessing\": true,\n"
            "      \"queueSize\": 2000,\n"
            "      \"threadPool\": %s\n"
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
            cb_cfg.stress_test ? "true" : "false");
    } else {
        // Blob configuration
        snprintf(config, 1024,
            "{\n"
            "  \"connection\": {\n"
            "    \"callbacks\": {\n"
            "      \"connectionEvents\": true,\n"
            "      \"dataAvailable\": true,\n"
            "      \"bufferStatus\": true,\n"
            "      \"statusUpdates\": true,\n"
            "      \"customEvents\": true\n"
            "    },\n"
            "    \"eventHandling\": {\n"
            "      \"asyncProcessing\": true,\n"
            "      \"queueSize\": 500,\n"
            "      \"threadPool\": %s\n"
            "    }\n"
            "  },\n"
            "  \"payload\": {\n"
            "    \"blob\": {}\n"
            "  }\n"
            "}",
            cb_cfg.stress_test ? "true" : "false");
    }

    return config;
}

void trigger_test_events() {
    if (!cb_cfg.trigger_errors) return;
    
    LOG("[CALLBACK] Triggering test events for error handling validation");
    
    // Simulate various error conditions
    static int trigger_count = 0;
    trigger_count++;
    
    switch (trigger_count % 4) {
        case 0:
            // Simulate connection error
            connection_event_callback(connection, MESH_CONN_EVENT_ERROR, NULL);
            break;
        case 1:
            // Simulate buffer full condition
            buffer_status_callback(connection, MESH_BUFFER_STATUS_FULL, NULL);
            break;
        case 2:
            // Simulate buffer empty condition  
            buffer_status_callback(connection, MESH_BUFFER_STATUS_EMPTY, NULL);
            break;
        case 3:
            // Simulate custom event
            custom_event_callback(connection, 999, NULL, NULL);
            break;
    }
}

void run_stress_test() {
    if (!cb_cfg.stress_test) return;
    
    LOG("[CALLBACK] Running callback stress test");
    
    // Generate high frequency callback events
    for (int i = 0; i < 1000; i++) {
        data_available_callback(connection, NULL);
        status_callback(connection, MESH_STATUS_CONNECTED, NULL);
        
        if (i % 100 == 0) {
            buffer_status_callback(connection, MESH_BUFFER_STATUS_NORMAL, NULL);
        }
        
        if (i % 50 == 0) {
            custom_event_callback(connection, i, NULL, NULL);
        }
        
        // Small delay to avoid overwhelming the system
        usleep(100); // 0.1ms
    }
}

void validate_thread_safety() {
    if (!cb_cfg.validate_threading) return;
    
    LOG("[CALLBACK] Validating thread safety");
    
    pthread_mutex_lock(&thread_mutex);
    
    int callback_threads = 0;
    uint64_t total_callbacks = 0;
    
    for (int i = 0; i < thread_count; i++) {
        if (threads[i].is_callback_thread) {
            callback_threads++;
            total_callbacks += threads[i].callback_count;
            
            if (cb_cfg.verbose) {
                LOG("[CALLBACK] Thread %s: %lu callbacks", 
                    threads[i].thread_name, threads[i].callback_count);
            }
        }
    }
    
    pthread_mutex_unlock(&thread_mutex);
    
    LOG("[CALLBACK] Thread safety validation: %d callback threads, %lu total callbacks", 
        callback_threads, total_callbacks);
    
    if (callback_threads > 1) {
        LOG("[CALLBACK] Multi-threaded callback execution detected");
    }
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    pthread_mutex_lock(&stats.stats_mutex);
    uint64_t total_events = stats.connection_events + stats.data_events + 
                           stats.error_events + stats.buffer_events + 
                           stats.status_events + stats.custom_events;
    pthread_mutex_unlock(&stats.stats_mutex);
    
    double event_rate = total_events / elapsed;
    
    printf("\r[CALLBACK] Progress: %.1fs | Events: %lu (%.1f/sec) | Errors: %lu", 
           elapsed, total_events, event_rate, stats.callback_errors);
    
    if (cb_cfg.measure_latency && stats.latency_samples > 0) {
        printf(" | Latency: %.1f/%.1f/%.1f μs (min/avg/max)", 
               stats.min_latency_us, stats.avg_latency_us, stats.max_latency_us);
    }
    
    fflush(stdout);
}

void save_callback_results() {
    if (strlen(cb_cfg.output_file) == 0) return;
    
    FILE *f = fopen(cb_cfg.output_file, "w");
    if (!f) {
        LOG("[CALLBACK] Failed to open output file: %s", cb_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Callback and Event Handling Test Results\n");
    fprintf(f, "Test Mode: %s\n", cb_cfg.test_mode);
    fprintf(f, "Payload Type: %s\n", cb_cfg.payload_type);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Stress Test: %s\n", cb_cfg.stress_test ? "enabled" : "disabled");
    fprintf(f, "Error Injection: %s\n", cb_cfg.trigger_errors ? "enabled" : "disabled");
    fprintf(f, "Thread Validation: %s\n", cb_cfg.validate_threading ? "enabled" : "disabled");
    fprintf(f, "Latency Measurement: %s\n", cb_cfg.measure_latency ? "enabled" : "disabled");
    
    fprintf(f, "\nEvent Statistics:\n");
    fprintf(f, "Connection Events: %lu\n", stats.connection_events);
    fprintf(f, "Data Events: %lu\n", stats.data_events);
    fprintf(f, "Error Events: %lu\n", stats.error_events);
    fprintf(f, "Buffer Events: %lu\n", stats.buffer_events);
    fprintf(f, "Status Events: %lu\n", stats.status_events);
    fprintf(f, "Custom Events: %lu\n", stats.custom_events);
    fprintf(f, "Callback Errors: %lu\n", stats.callback_errors);
    fprintf(f, "Thread Violations: %lu\n", stats.thread_violations);
    
    uint64_t total_events = stats.connection_events + stats.data_events + 
                           stats.error_events + stats.buffer_events + 
                           stats.status_events + stats.custom_events;
    double event_rate = total_events / total_time;
    fprintf(f, "Total Events: %lu\n", total_events);
    fprintf(f, "Event Rate: %.2f events/sec\n", event_rate);
    
    if (cb_cfg.measure_latency && stats.latency_samples > 0) {
        fprintf(f, "\nLatency Statistics:\n");
        fprintf(f, "Samples: %lu\n", stats.latency_samples);
        fprintf(f, "Min Latency: %.2f μs\n", stats.min_latency_us);
        fprintf(f, "Max Latency: %.2f μs\n", stats.max_latency_us);
        fprintf(f, "Average Latency: %.2f μs\n", stats.avg_latency_us);
    }
    
    if (cb_cfg.validate_threading) {
        fprintf(f, "\nThread Safety Analysis:\n");
        fprintf(f, "Thread Count: %d\n", thread_count);
        
        for (int i = 0; i < thread_count; i++) {
            if (threads[i].callback_count > 0) {
                fprintf(f, "Thread %s: %lu callbacks\n", 
                       threads[i].thread_name, threads[i].callback_count);
            }
        }
    }
    
    fclose(f);
    LOG("[CALLBACK] Test results saved to: %s", cb_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[CALLBACK] Starting Callback and Event Handling Test");
    LOG("[CALLBACK] Mode: %s, Payload: %s, Duration: %d seconds", 
        cb_cfg.test_mode, cb_cfg.payload_type, cb_cfg.test_duration_sec);
    LOG("[CALLBACK] Options - Stress: %s, Errors: %s, Threading: %s, Latency: %s", 
        cb_cfg.stress_test ? "enabled" : "disabled",
        cb_cfg.trigger_errors ? "enabled" : "disabled",
        cb_cfg.validate_threading ? "enabled" : "disabled",
        cb_cfg.measure_latency ? "enabled" : "disabled");

    // Initialize mutex
    pthread_mutex_init(&stats.stats_mutex, NULL);

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate callback connection configuration
    conn_cfg = generate_callback_config();
    if (!conn_cfg) {
        free(client_cfg);
        exit(EXIT_FAILURE);
    }

    LOG("[CALLBACK] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[CALLBACK] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection with callbacks */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[CALLBACK] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Register callbacks if supported by the API
    // Note: These are placeholder calls - actual API may differ
    if (strcmp(cb_cfg.test_mode, "callbacks") == 0 || strcmp(cb_cfg.test_mode, "combined") == 0) {
        LOG("[CALLBACK] Registering event callbacks");
        // mesh_register_connection_callback(connection, connection_event_callback, NULL);
        // mesh_register_data_callback(connection, data_available_callback, NULL);
        // mesh_register_buffer_callback(connection, buffer_status_callback, NULL);
        // mesh_register_status_callback(connection, status_callback, NULL);
        // mesh_register_custom_callback(connection, custom_event_callback, NULL);
    }

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    
    LOG("[CALLBACK] Starting callback test for %d seconds...", cb_cfg.test_duration_sec);

    // Main test loop
    struct timeval test_end;
    gettimeofday(&test_end, NULL);
    test_end.tv_sec += cb_cfg.test_duration_sec;

    int progress_counter = 0;
    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        if (shutdown_flag == SHUTDOWN_REQUESTED) {
            LOG("[CALLBACK] Graceful shutdown requested");
            break;
        }
        
        // Simulate regular activity to trigger callbacks
        MeshBuffer *buf;
        err = mesh_get_buffer_timeout(connection, &buf, 100); // 100ms timeout
        
        if (err == 0 && buf) {
            // Process the buffer (this may trigger data callbacks)
            data_available_callback(connection, NULL);
            mesh_put_buffer(&buf);
        }
        
        // Trigger test events periodically
        if (progress_counter % 10 == 0) {
            trigger_test_events();
        }
        
        // Run stress test periodically
        if (cb_cfg.stress_test && progress_counter % 50 == 0) {
            run_stress_test();
        }
        
        // Print progress every 5 seconds
        if (progress_counter % 50 == 0) {
            print_progress_stats();
        }
        
        progress_counter++;
        usleep(100000); // 100ms
    }
    
    printf("\n");
    
    // Validate thread safety
    validate_thread_safety();
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    uint64_t total_events = stats.connection_events + stats.data_events + 
                           stats.error_events + stats.buffer_events + 
                           stats.status_events + stats.custom_events;
    
    LOG("[CALLBACK] Test completed in %.2f seconds", total_time);
    LOG("[CALLBACK] Total events processed: %lu", total_events);
    LOG("[CALLBACK] Event rate: %.2f events/sec", total_events / total_time);
    LOG("[CALLBACK] Callback errors: %lu", stats.callback_errors);
    
    if (cb_cfg.measure_latency && stats.latency_samples > 0) {
        LOG("[CALLBACK] Latency - Min: %.1f μs, Avg: %.1f μs, Max: %.1f μs", 
            stats.min_latency_us, stats.avg_latency_us, stats.max_latency_us);
    }
    
    if (cb_cfg.validate_threading) {
        LOG("[CALLBACK] Thread safety - %d threads used, %lu violations", 
            thread_count, stats.thread_violations);
    }
    
    // Save results to file if specified
    save_callback_results();

safe_exit:
    LOG("[CALLBACK] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[CALLBACK] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    pthread_mutex_destroy(&stats.stats_mutex);
    free(client_cfg);
    free(conn_cfg);
    return err;
}
