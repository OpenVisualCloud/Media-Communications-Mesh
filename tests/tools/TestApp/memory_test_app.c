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
#include <sys/resource.h>
#include <pthread.h>

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

// Memory management test configuration
typedef struct {
    char test_mode[32];          // "leak", "fragmentation", "stress", "all"
    char payload_type[16];       // "video", "audio", "blob"
    int test_duration_sec;
    int allocation_rate;         // Allocations per second
    int max_allocations;         // Maximum concurrent allocations
    int stress_patterns;         // Enable stress test patterns
    int track_leaks;            // Enable leak detection
    int validate_alignment;      // Check memory alignment
    int test_boundaries;         // Test buffer boundaries
    char output_file[256];
    int verbose;
} memory_config_t;

static memory_config_t mem_cfg = {
    .test_mode = "all",
    .payload_type = "video",
    .test_duration_sec = 300,  // 5 minutes
    .allocation_rate = 100,     // 100 allocs/sec
    .max_allocations = 10000,
    .stress_patterns = 1,
    .track_leaks = 1,
    .validate_alignment = 1,
    .test_boundaries = 1,
    .output_file = "",
    .verbose = 0
};

// Memory tracking structure
typedef struct allocation {
    void *ptr;
    size_t size;
    struct timeval allocated_time;
    const char *function;
    int line;
    struct allocation *next;
} allocation_t;

// Memory statistics
typedef struct {
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint64_t peak_memory_usage;
    uint64_t current_memory_usage;
    uint64_t bytes_allocated;
    uint64_t bytes_deallocated;
    uint64_t allocation_failures;
    uint64_t deallocation_errors;
    uint64_t alignment_violations;
    uint64_t boundary_violations;
    uint64_t memory_leaks;
    double avg_allocation_size;
    double fragmentation_ratio;
    struct timeval start_time;
    allocation_t *allocation_list;
    pthread_mutex_t memory_mutex;
} memory_stats_t;

static memory_stats_t stats = {0};

// Test allocation tracking
static void **test_allocations = NULL;
static size_t *allocation_sizes = NULL;
static int allocation_count = 0;

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Memory Management Validation Test Application\n\n");
    printf("Test Configuration:\n");
    printf("  -m, --mode <type>          Test mode: leak, fragmentation, stress, all (default: %s)\n", mem_cfg.test_mode);
    printf("  -p, --payload <type>       Payload type: video, audio, blob (default: %s)\n", mem_cfg.payload_type);
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", mem_cfg.test_duration_sec);
    printf("\n  Memory Testing:\n");
    printf("  --allocation-rate <rate>   Allocations per second (default: %d)\n", mem_cfg.allocation_rate);
    printf("  --max-allocations <count>  Maximum concurrent allocations (default: %d)\n", mem_cfg.max_allocations);
    printf("  --stress-patterns          Enable stress test patterns (default: %s)\n", mem_cfg.stress_patterns ? "enabled" : "disabled");
    printf("  --track-leaks              Enable leak detection (default: %s)\n", mem_cfg.track_leaks ? "enabled" : "disabled");
    printf("  --validate-alignment       Check memory alignment (default: %s)\n", mem_cfg.validate_alignment ? "enabled" : "disabled");
    printf("  --test-boundaries          Test buffer boundaries (default: %s)\n", mem_cfg.test_boundaries ? "enabled" : "disabled");
    printf("  --no-stress                Disable stress patterns\n");
    printf("  --no-leak-tracking         Disable leak detection\n");
    printf("  --no-alignment             Disable alignment validation\n");
    printf("  --no-boundaries            Disable boundary testing\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Test memory leaks\n");
    printf("    %s --mode leak --track-leaks\n", prog_name);
    printf("\n    # Memory fragmentation testing\n");
    printf("    %s --mode fragmentation --stress-patterns\n", prog_name);
    printf("\n    # Comprehensive memory stress test\n");
    printf("    %s --mode stress --allocation-rate 1000 --max-allocations 50000\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"payload", required_argument, 0, 'p'},
        {"test-duration", required_argument, 0, 't'},
        {"allocation-rate", required_argument, 0, 1001},
        {"max-allocations", required_argument, 0, 1002},
        {"stress-patterns", no_argument, 0, 1003},
        {"track-leaks", no_argument, 0, 1004},
        {"validate-alignment", no_argument, 0, 1005},
        {"test-boundaries", no_argument, 0, 1006},
        {"no-stress", no_argument, 0, 1007},
        {"no-leak-tracking", no_argument, 0, 1008},
        {"no-alignment", no_argument, 0, 1009},
        {"no-boundaries", no_argument, 0, 1010},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "m:p:t:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'm':
                strncpy(mem_cfg.test_mode, optarg, sizeof(mem_cfg.test_mode) - 1);
                break;
            case 'p':
                strncpy(mem_cfg.payload_type, optarg, sizeof(mem_cfg.payload_type) - 1);
                break;
            case 't':
                mem_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1001:
                mem_cfg.allocation_rate = atoi(optarg);
                break;
            case 1002:
                mem_cfg.max_allocations = atoi(optarg);
                break;
            case 1003:
                mem_cfg.stress_patterns = 1;
                break;
            case 1004:
                mem_cfg.track_leaks = 1;
                break;
            case 1005:
                mem_cfg.validate_alignment = 1;
                break;
            case 1006:
                mem_cfg.test_boundaries = 1;
                break;
            case 1007:
                mem_cfg.stress_patterns = 0;
                break;
            case 1008:
                mem_cfg.track_leaks = 0;
                break;
            case 1009:
                mem_cfg.validate_alignment = 0;
                break;
            case 1010:
                mem_cfg.test_boundaries = 0;
                break;
            case 'o':
                strncpy(mem_cfg.output_file, optarg, sizeof(mem_cfg.output_file) - 1);
                break;
            case 'v':
                mem_cfg.verbose = 1;
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

// Track memory allocation for leak detection
void track_allocation(void *ptr, size_t size, const char *function, int line) {
    if (!mem_cfg.track_leaks || !ptr) return;
    
    pthread_mutex_lock(&stats.memory_mutex);
    
    allocation_t *alloc = malloc(sizeof(allocation_t));
    if (alloc) {
        alloc->ptr = ptr;
        alloc->size = size;
        gettimeofday(&alloc->allocated_time, NULL);
        alloc->function = function;
        alloc->line = line;
        alloc->next = stats.allocation_list;
        stats.allocation_list = alloc;
        
        stats.current_memory_usage += size;
        if (stats.current_memory_usage > stats.peak_memory_usage) {
            stats.peak_memory_usage = stats.current_memory_usage;
        }
    }
    
    pthread_mutex_unlock(&stats.memory_mutex);
}

// Track memory deallocation
void track_deallocation(void *ptr) {
    if (!mem_cfg.track_leaks || !ptr) return;
    
    pthread_mutex_lock(&stats.memory_mutex);
    
    allocation_t **current = &stats.allocation_list;
    while (*current) {
        if ((*current)->ptr == ptr) {
            allocation_t *to_free = *current;
            *current = (*current)->next;
            
            stats.current_memory_usage -= to_free->size;
            free(to_free);
            pthread_mutex_unlock(&stats.memory_mutex);
            return;
        }
        current = &(*current)->next;
    }
    
    // Pointer not found - potential double free or corruption
    stats.deallocation_errors++;
    
    pthread_mutex_unlock(&stats.memory_mutex);
}

// Validate memory alignment
int validate_alignment(void *ptr, size_t alignment) {
    if (!mem_cfg.validate_alignment || !ptr) return 1;
    
    uintptr_t addr = (uintptr_t)ptr;
    if ((addr % alignment) != 0) {
        stats.alignment_violations++;
        if (mem_cfg.verbose) {
            LOG("[MEM] Alignment violation: %p not aligned to %zu bytes", ptr, alignment);
        }
        return 0;
    }
    
    return 1;
}

// Test buffer boundaries
int test_buffer_boundaries(void *ptr, size_t size) {
    if (!mem_cfg.test_boundaries || !ptr || size == 0) return 1;
    
    uint8_t *buffer = (uint8_t*)ptr;
    
    // Write test pattern to boundaries
    uint8_t start_pattern = 0xAA;
    uint8_t end_pattern = 0x55;
    
    // Test start boundary
    uint8_t original_start = buffer[0];
    buffer[0] = start_pattern;
    if (buffer[0] != start_pattern) {
        stats.boundary_violations++;
        if (mem_cfg.verbose) {
            LOG("[MEM] Start boundary violation at %p", ptr);
        }
        return 0;
    }
    buffer[0] = original_start;
    
    // Test end boundary
    uint8_t original_end = buffer[size - 1];
    buffer[size - 1] = end_pattern;
    if (buffer[size - 1] != end_pattern) {
        stats.boundary_violations++;
        if (mem_cfg.verbose) {
            LOG("[MEM] End boundary violation at %p+%zu", ptr, size - 1);
        }
        return 0;
    }
    buffer[size - 1] = original_end;
    
    return 1;
}

// Wrapped allocation function
void* test_malloc(size_t size, const char *function, int line) {
    void *ptr = malloc(size);
    
    pthread_mutex_lock(&stats.memory_mutex);
    stats.total_allocations++;
    stats.bytes_allocated += size;
    
    if (stats.total_allocations > 0) {
        stats.avg_allocation_size = (double)stats.bytes_allocated / stats.total_allocations;
    }
    pthread_mutex_unlock(&stats.memory_mutex);
    
    if (ptr) {
        track_allocation(ptr, size, function, line);
        validate_alignment(ptr, sizeof(void*));
        test_buffer_boundaries(ptr, size);
    } else {
        pthread_mutex_lock(&stats.memory_mutex);
        stats.allocation_failures++;
        pthread_mutex_unlock(&stats.memory_mutex);
    }
    
    return ptr;
}

// Wrapped deallocation function
void test_free(void *ptr) {
    if (!ptr) return;
    
    track_deallocation(ptr);
    
    pthread_mutex_lock(&stats.memory_mutex);
    stats.total_deallocations++;
    pthread_mutex_unlock(&stats.memory_mutex);
    
    free(ptr);
}

#define TEST_MALLOC(size) test_malloc(size, __FUNCTION__, __LINE__)
#define TEST_FREE(ptr) test_free(ptr)

// Memory leak testing
void test_memory_leaks() {
    LOG("[MEM] Testing memory leaks");
    
    int leak_count = 100;
    for (int i = 0; i < leak_count; i++) {
        size_t size = 1024 + (rand() % 4096);
        void *ptr = TEST_MALLOC(size);
        
        if (ptr) {
            // Intentionally leak every 10th allocation
            if (i % 10 == 0) {
                if (mem_cfg.verbose) {
                    LOG("[MEM] Intentionally leaking allocation %d (%zu bytes)", i, size);
                }
                // Don't free - this creates a leak for testing
            } else {
                TEST_FREE(ptr);
            }
        }
        
        usleep(10000); // 10ms
    }
}

// Memory fragmentation testing
void test_memory_fragmentation() {
    LOG("[MEM] Testing memory fragmentation");
    
    int alloc_count = 1000;
    void **ptrs = malloc(alloc_count * sizeof(void*));
    size_t *sizes = malloc(alloc_count * sizeof(size_t));
    
    if (!ptrs || !sizes) {
        free(ptrs);
        free(sizes);
        return;
    }
    
    // Allocate varying sizes to create fragmentation
    for (int i = 0; i < alloc_count; i++) {
        // Vary allocation sizes significantly
        if (i % 4 == 0) sizes[i] = 64;           // Small
        else if (i % 4 == 1) sizes[i] = 1024;    // Medium
        else if (i % 4 == 2) sizes[i] = 16384;   // Large
        else sizes[i] = 1048576;                 // Very large
        
        ptrs[i] = TEST_MALLOC(sizes[i]);
    }
    
    // Free in random order to create fragmentation
    for (int i = 0; i < alloc_count; i++) {
        int idx = rand() % alloc_count;
        if (ptrs[idx]) {
            TEST_FREE(ptrs[idx]);
            ptrs[idx] = NULL;
        }
    }
    
    // Calculate fragmentation ratio
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    long max_rss = usage.ru_maxrss * 1024; // Convert to bytes on Linux
    
    pthread_mutex_lock(&stats.memory_mutex);
    if (stats.peak_memory_usage > 0) {
        stats.fragmentation_ratio = (double)max_rss / stats.peak_memory_usage;
    }
    pthread_mutex_unlock(&stats.memory_mutex);
    
    free(ptrs);
    free(sizes);
}

// Memory stress testing
void test_memory_stress() {
    LOG("[MEM] Running memory stress test");
    
    // Initialize test allocations array
    test_allocations = malloc(mem_cfg.max_allocations * sizeof(void*));
    allocation_sizes = malloc(mem_cfg.max_allocations * sizeof(size_t));
    
    if (!test_allocations || !allocation_sizes) {
        free(test_allocations);
        free(allocation_sizes);
        return;
    }
    
    memset(test_allocations, 0, mem_cfg.max_allocations * sizeof(void*));
    
    struct timeval start, last_alloc;
    gettimeofday(&start, NULL);
    last_alloc = start;
    
    int allocation_counter = 0;
    double alloc_interval_us = 1000000.0 / mem_cfg.allocation_rate; // microseconds between allocations
    
    for (int duration = 0; duration < mem_cfg.test_duration_sec; duration++) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        // Check if it's time for next allocation
        double elapsed_us = ((now.tv_sec - last_alloc.tv_sec) * 1000000.0) +
                           (now.tv_usec - last_alloc.tv_usec);
        
        if (elapsed_us >= alloc_interval_us && allocation_count < mem_cfg.max_allocations) {
            // Allocate with varying sizes
            size_t size;
            if (mem_cfg.stress_patterns) {
                // Stress patterns: very small to very large
                int pattern = rand() % 5;
                switch (pattern) {
                    case 0: size = 16; break;           // Very small
                    case 1: size = 1024; break;         // Small
                    case 2: size = 64 * 1024; break;    // Medium
                    case 3: size = 1024 * 1024; break;  // Large
                    case 4: size = 10 * 1024 * 1024; break; // Very large
                    default: size = 4096; break;
                }
            } else {
                size = 1024 + (rand() % 8192); // 1KB to 9KB
            }
            
            void *ptr = TEST_MALLOC(size);
            if (ptr) {
                test_allocations[allocation_count] = ptr;
                allocation_sizes[allocation_count] = size;
                allocation_count++;
                
                // Fill memory to ensure it's actually allocated
                memset(ptr, 0x42, size);
            }
            
            last_alloc = now;
        }
        
        // Randomly deallocate some memory
        if (allocation_count > 100 && (rand() % 100) < 20) { // 20% chance
            int idx = rand() % allocation_count;
            if (test_allocations[idx]) {
                TEST_FREE(test_allocations[idx]);
                
                // Move last allocation to this slot
                test_allocations[idx] = test_allocations[allocation_count - 1];
                allocation_sizes[idx] = allocation_sizes[allocation_count - 1];
                allocation_count--;
            }
        }
        
        usleep(100000); // 100ms
    }
    
    // Clean up remaining allocations
    for (int i = 0; i < allocation_count; i++) {
        if (test_allocations[i]) {
            TEST_FREE(test_allocations[i]);
        }
    }
    
    free(test_allocations);
    free(allocation_sizes);
}

// Check for memory leaks
void check_memory_leaks() {
    if (!mem_cfg.track_leaks) return;
    
    pthread_mutex_lock(&stats.memory_mutex);
    
    allocation_t *current = stats.allocation_list;
    while (current) {
        stats.memory_leaks++;
        
        struct timeval now;
        gettimeofday(&now, NULL);
        double age_ms = ((now.tv_sec - current->allocated_time.tv_sec) * 1000.0) +
                       ((now.tv_usec - current->allocated_time.tv_usec) / 1000.0);
        
        if (mem_cfg.verbose) {
            LOG("[MEM] Memory leak detected: %zu bytes at %p, allocated in %s:%d (%.1f ms ago)",
                current->size, current->ptr, current->function, current->line, age_ms);
        }
        
        current = current->next;
    }
    
    pthread_mutex_unlock(&stats.memory_mutex);
}

char* generate_memory_config() {
    char *config = malloc(1024);
    if (!config) {
        LOG("[MEM] Failed to allocate memory for config");
        return NULL;
    }

    snprintf(config, 1024,
        "{\n"
        "  \"connection\": {\n"
        "    \"memoryManagement\": {\n"
        "      \"bufferPoolSize\": %d,\n"
        "      \"enablePreallocation\": true,\n"
        "      \"memoryAlignment\": 64,\n"
        "      \"enableLeakDetection\": %s,\n"
        "      \"fragmentationThreshold\": 0.8\n"
        "    },\n"
        "    \"performance\": {\n"
        "      \"zeroCopy\": true,\n"
        "      \"memoryMapping\": true\n"
        "    }\n"
        "  },\n"
        "  \"payload\": {\n"
        "    \"%s\": %s\n"
        "  }\n"
        "}",
        mem_cfg.max_allocations / 10,
        mem_cfg.track_leaks ? "true" : "false",
        mem_cfg.payload_type,
        strcmp(mem_cfg.payload_type, "video") == 0 ? 
            "{ \"width\": 1920, \"height\": 1080, \"fps\": 30, \"pixelFormat\": \"yuv422p10le\" }" :
        strcmp(mem_cfg.payload_type, "audio") == 0 ?
            "{ \"channels\": 2, \"sampleRate\": 48000, \"format\": \"pcm_s16le\" }" :
            "{}");

    return config;
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    pthread_mutex_lock(&stats.memory_mutex);
    double current_mb = stats.current_memory_usage / (1024.0 * 1024.0);
    double peak_mb = stats.peak_memory_usage / (1024.0 * 1024.0);
    pthread_mutex_unlock(&stats.memory_mutex);
    
    printf("\r[MEM] Progress: %.1fs | Allocs: %lu | Current: %.1f MB | Peak: %.1f MB | Leaks: %lu", 
           elapsed, stats.total_allocations, current_mb, peak_mb, stats.memory_leaks);
    
    if (stats.allocation_failures > 0) {
        printf(" | Failures: %lu", stats.allocation_failures);
    }
    
    if (stats.alignment_violations > 0 || stats.boundary_violations > 0) {
        printf(" | Violations: %lu/%lu", stats.alignment_violations, stats.boundary_violations);
    }
    
    fflush(stdout);
}

void save_memory_results() {
    if (strlen(mem_cfg.output_file) == 0) return;
    
    FILE *f = fopen(mem_cfg.output_file, "w");
    if (!f) {
        LOG("[MEM] Failed to open output file: %s", mem_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Memory Management Validation Test Results\n");
    fprintf(f, "Test Mode: %s\n", mem_cfg.test_mode);
    fprintf(f, "Payload Type: %s\n", mem_cfg.payload_type);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Allocation Rate: %d allocs/sec\n", mem_cfg.allocation_rate);
    fprintf(f, "Max Allocations: %d\n", mem_cfg.max_allocations);
    
    fprintf(f, "\nAllocation Statistics:\n");
    fprintf(f, "Total Allocations: %lu\n", stats.total_allocations);
    fprintf(f, "Total Deallocations: %lu\n", stats.total_deallocations);
    fprintf(f, "Allocation Failures: %lu\n", stats.allocation_failures);
    fprintf(f, "Deallocation Errors: %lu\n", stats.deallocation_errors);
    fprintf(f, "Bytes Allocated: %lu\n", stats.bytes_allocated);
    fprintf(f, "Bytes Deallocated: %lu\n", stats.bytes_deallocated);
    fprintf(f, "Average Allocation Size: %.2f bytes\n", stats.avg_allocation_size);
    
    fprintf(f, "\nMemory Usage:\n");
    fprintf(f, "Peak Memory Usage: %.2f MB\n", stats.peak_memory_usage / (1024.0 * 1024.0));
    fprintf(f, "Current Memory Usage: %.2f MB\n", stats.current_memory_usage / (1024.0 * 1024.0));
    fprintf(f, "Fragmentation Ratio: %.2f\n", stats.fragmentation_ratio);
    
    fprintf(f, "\nValidation Results:\n");
    fprintf(f, "Memory Leaks: %lu\n", stats.memory_leaks);
    fprintf(f, "Alignment Violations: %lu\n", stats.alignment_violations);
    fprintf(f, "Boundary Violations: %lu\n", stats.boundary_violations);
    
    double alloc_rate = stats.total_allocations / total_time;
    double success_rate = (stats.total_allocations > 0) ? 
        (double)((stats.total_allocations - stats.allocation_failures) * 100) / stats.total_allocations : 0.0;
    
    fprintf(f, "\nPerformance Metrics:\n");
    fprintf(f, "Allocation Rate: %.2f allocs/sec\n", alloc_rate);
    fprintf(f, "Allocation Success Rate: %.2f%%\n", success_rate);
    
    fclose(f);
    LOG("[MEM] Test results saved to: %s", mem_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[MEM] Starting Memory Management Validation Test");
    LOG("[MEM] Mode: %s, Payload: %s, Duration: %d seconds", 
        mem_cfg.test_mode, mem_cfg.payload_type, mem_cfg.test_duration_sec);
    LOG("[MEM] Rate: %d allocs/sec, Max: %d, Options: %s%s%s%s", 
        mem_cfg.allocation_rate, mem_cfg.max_allocations,
        mem_cfg.stress_patterns ? "stress " : "",
        mem_cfg.track_leaks ? "leaks " : "",
        mem_cfg.validate_alignment ? "align " : "",
        mem_cfg.test_boundaries ? "bounds " : "");

    // Initialize mutex
    pthread_mutex_init(&stats.memory_mutex, NULL);
    
    // Initialize random seed
    srand(time(NULL));

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate memory-aware connection configuration
    conn_cfg = generate_memory_config();
    if (!conn_cfg) {
        free(client_cfg);
        exit(EXIT_FAILURE);
    }

    LOG("[MEM] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[MEM] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[MEM] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    
    LOG("[MEM] Starting memory tests...");

    // Run specific tests based on mode
    if (strcmp(mem_cfg.test_mode, "leak") == 0) {
        test_memory_leaks();
    } else if (strcmp(mem_cfg.test_mode, "fragmentation") == 0) {
        test_memory_fragmentation();
    } else if (strcmp(mem_cfg.test_mode, "stress") == 0) {
        test_memory_stress();
    } else if (strcmp(mem_cfg.test_mode, "all") == 0) {
        // Run all tests
        test_memory_leaks();
        usleep(1000000); // 1 second between tests
        test_memory_fragmentation();
        usleep(1000000);
        test_memory_stress();
    }
    
    // Regular connection activity during tests
    struct timeval test_end;
    gettimeofday(&test_end, NULL);
    test_end.tv_sec += mem_cfg.test_duration_sec;
    
    int progress_counter = 0;
    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        if (shutdown_flag == SHUTDOWN_REQUESTED) {
            LOG("[MEM] Graceful shutdown requested");
            break;
        }
        
        // Regular mesh operations to test memory usage
        MeshBuffer *buf;
        err = mesh_get_buffer_timeout(connection, &buf, 100);
        
        if (err == 0 && buf) {
            // Test buffer memory properties
            validate_alignment(buf->payload_ptr, 64); // 64-byte alignment
            test_buffer_boundaries(buf->payload_ptr, buf->payload_len);
            
            mesh_put_buffer(&buf);
        }
        
        // Print progress every 10 seconds
        if (progress_counter % 100 == 0) {
            print_progress_stats();
        }
        
        progress_counter++;
        usleep(100000); // 100ms
    }
    
    printf("\n");
    
    // Check for memory leaks
    check_memory_leaks();
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[MEM] Test completed in %.2f seconds", total_time);
    LOG("[MEM] Total allocations: %lu", stats.total_allocations);
    LOG("[MEM] Peak memory usage: %.2f MB", stats.peak_memory_usage / (1024.0 * 1024.0));
    LOG("[MEM] Memory leaks detected: %lu", stats.memory_leaks);
    LOG("[MEM] Allocation failures: %lu", stats.allocation_failures);
    LOG("[MEM] Alignment violations: %lu", stats.alignment_violations);
    LOG("[MEM] Boundary violations: %lu", stats.boundary_violations);
    
    if (stats.fragmentation_ratio > 0) {
        LOG("[MEM] Memory fragmentation ratio: %.2f", stats.fragmentation_ratio);
    }
    
    double success_rate = (stats.total_allocations > 0) ? 
        (double)((stats.total_allocations - stats.allocation_failures) * 100) / stats.total_allocations : 0.0;
    LOG("[MEM] Allocation success rate: %.1f%%", success_rate);
    
    // Save results to file if specified
    save_memory_results();

safe_exit:
    LOG("[MEM] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[MEM] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    
    // Clean up leak tracking
    pthread_mutex_lock(&stats.memory_mutex);
    allocation_t *current = stats.allocation_list;
    while (current) {
        allocation_t *next = current->next;
        free(current);
        current = next;
    }
    pthread_mutex_unlock(&stats.memory_mutex);
    
    pthread_mutex_destroy(&stats.memory_mutex);
    free(client_cfg);
    free(conn_cfg);
    return err;
}
