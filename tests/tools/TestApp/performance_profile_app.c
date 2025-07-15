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

// Performance profiling configuration
typedef struct {
    char profile_mode[32];       // "throughput", "latency", "cpu", "memory", "all"
    char payload_type[16];       // "video", "audio", "blob"
    int test_duration_sec;
    int sample_interval_ms;      // Profiling sample interval
    int enable_cpu_profiling;    // CPU usage profiling
    int enable_memory_profiling; // Memory usage profiling
    int enable_io_profiling;     // I/O profiling
    int enable_network_profiling; // Network profiling
    int detailed_analysis;       // Detailed performance analysis
    char output_file[256];
    char csv_output[256];
    int verbose;
} profile_config_t;

static profile_config_t prof_cfg = {
    .profile_mode = "all",
    .payload_type = "video",
    .test_duration_sec = 300,  // 5 minutes
    .sample_interval_ms = 1000, // 1 second samples
    .enable_cpu_profiling = 1,
    .enable_memory_profiling = 1,
    .enable_io_profiling = 1,
    .enable_network_profiling = 1,
    .detailed_analysis = 1,
    .output_file = "",
    .csv_output = "",
    .verbose = 0
};

// Performance sample structure
typedef struct {
    struct timeval timestamp;
    double cpu_usage_percent;
    uint64_t memory_rss_kb;
    uint64_t memory_vss_kb;
    uint64_t network_bytes_rx;
    uint64_t network_bytes_tx;
    uint64_t io_reads;
    uint64_t io_writes;
    uint64_t context_switches;
    double throughput_mbps;
    double latency_us;
    uint64_t packets_processed;
    uint64_t errors_detected;
} perf_sample_t;

// Performance statistics
typedef struct {
    perf_sample_t *samples;
    int sample_count;
    int max_samples;
    
    // Aggregate statistics
    double avg_cpu_usage;
    double peak_cpu_usage;
    uint64_t peak_memory_kb;
    double avg_throughput_mbps;
    double peak_throughput_mbps;
    double avg_latency_us;
    double min_latency_us;
    double max_latency_us;
    uint64_t total_packets;
    uint64_t total_errors;
    
    // Performance trends
    double cpu_trend;           // CPU usage trend (positive = increasing)
    double memory_trend;        // Memory usage trend
    double throughput_trend;    // Throughput trend
    
    struct timeval start_time;
    pthread_mutex_t perf_mutex;
} perf_stats_t;

static perf_stats_t stats = {0};

// Baseline measurements for comparison
static struct {
    double baseline_cpu;
    uint64_t baseline_memory;
    struct timeval baseline_time;
    int initialized;
} baseline = {0};

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Performance Profiling Test Application\n\n");
    printf("Test Configuration:\n");
    printf("  -m, --mode <type>          Profile mode: throughput, latency, cpu, memory, all (default: %s)\n", prof_cfg.profile_mode);
    printf("  -p, --payload <type>       Payload type: video, audio, blob (default: %s)\n", prof_cfg.payload_type);
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", prof_cfg.test_duration_sec);
    printf("\n  Profiling Options:\n");
    printf("  --sample-interval <ms>     Sampling interval in ms (default: %d)\n", prof_cfg.sample_interval_ms);
    printf("  --enable-cpu               Enable CPU profiling (default: %s)\n", prof_cfg.enable_cpu_profiling ? "enabled" : "disabled");
    printf("  --enable-memory            Enable memory profiling (default: %s)\n", prof_cfg.enable_memory_profiling ? "enabled" : "disabled");
    printf("  --enable-io                Enable I/O profiling (default: %s)\n", prof_cfg.enable_io_profiling ? "enabled" : "disabled");
    printf("  --enable-network           Enable network profiling (default: %s)\n", prof_cfg.enable_network_profiling ? "enabled" : "disabled");
    printf("  --detailed-analysis        Enable detailed analysis (default: %s)\n", prof_cfg.detailed_analysis ? "enabled" : "disabled");
    printf("  --disable-cpu              Disable CPU profiling\n");
    printf("  --disable-memory           Disable memory profiling\n");
    printf("  --disable-io               Disable I/O profiling\n");
    printf("  --disable-network          Disable network profiling\n");
    printf("  --basic-analysis           Disable detailed analysis\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  --csv <file>               Save results in CSV format\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # CPU performance profiling\n");
    printf("    %s --mode cpu --enable-cpu --sample-interval 500\n", prog_name);
    printf("\n    # Throughput analysis\n");
    printf("    %s --mode throughput --detailed-analysis\n", prog_name);
    printf("\n    # Complete performance profile\n");
    printf("    %s --mode all --csv performance.csv\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"payload", required_argument, 0, 'p'},
        {"test-duration", required_argument, 0, 't'},
        {"sample-interval", required_argument, 0, 1001},
        {"enable-cpu", no_argument, 0, 1002},
        {"enable-memory", no_argument, 0, 1003},
        {"enable-io", no_argument, 0, 1004},
        {"enable-network", no_argument, 0, 1005},
        {"detailed-analysis", no_argument, 0, 1006},
        {"disable-cpu", no_argument, 0, 1007},
        {"disable-memory", no_argument, 0, 1008},
        {"disable-io", no_argument, 0, 1009},
        {"disable-network", no_argument, 0, 1010},
        {"basic-analysis", no_argument, 0, 1011},
        {"output", required_argument, 0, 'o'},
        {"csv", required_argument, 0, 1012},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "m:p:t:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'm':
                strncpy(prof_cfg.profile_mode, optarg, sizeof(prof_cfg.profile_mode) - 1);
                break;
            case 'p':
                strncpy(prof_cfg.payload_type, optarg, sizeof(prof_cfg.payload_type) - 1);
                break;
            case 't':
                prof_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1001:
                prof_cfg.sample_interval_ms = atoi(optarg);
                break;
            case 1002:
                prof_cfg.enable_cpu_profiling = 1;
                break;
            case 1003:
                prof_cfg.enable_memory_profiling = 1;
                break;
            case 1004:
                prof_cfg.enable_io_profiling = 1;
                break;
            case 1005:
                prof_cfg.enable_network_profiling = 1;
                break;
            case 1006:
                prof_cfg.detailed_analysis = 1;
                break;
            case 1007:
                prof_cfg.enable_cpu_profiling = 0;
                break;
            case 1008:
                prof_cfg.enable_memory_profiling = 0;
                break;
            case 1009:
                prof_cfg.enable_io_profiling = 0;
                break;
            case 1010:
                prof_cfg.enable_network_profiling = 0;
                break;
            case 1011:
                prof_cfg.detailed_analysis = 0;
                break;
            case 'o':
                strncpy(prof_cfg.output_file, optarg, sizeof(prof_cfg.output_file) - 1);
                break;
            case 1012:
                strncpy(prof_cfg.csv_output, optarg, sizeof(prof_cfg.csv_output) - 1);
                break;
            case 'v':
                prof_cfg.verbose = 1;
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

// Get CPU usage percentage
double get_cpu_usage() {
    static clock_t last_cpu = 0;
    static struct timeval last_time = {0, 0};
    
    clock_t current_cpu = clock();
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    if (last_cpu == 0) {
        last_cpu = current_cpu;
        last_time = current_time;
        return 0.0;
    }
    
    double cpu_time_used = ((double)(current_cpu - last_cpu)) / CLOCKS_PER_SEC;
    double real_time = ((current_time.tv_sec - last_time.tv_sec) +
                       (current_time.tv_usec - last_time.tv_usec) / 1000000.0);
    
    last_cpu = current_cpu;
    last_time = current_time;
    
    if (real_time > 0) {
        return (cpu_time_used / real_time) * 100.0;
    }
    
    return 0.0;
}

// Get memory usage from /proc/self/status
void get_memory_usage(uint64_t *rss_kb, uint64_t *vss_kb) {
    FILE *status = fopen("/proc/self/status", "r");
    if (!status) {
        *rss_kb = 0;
        *vss_kb = 0;
        return;
    }
    
    char line[256];
    *rss_kb = 0;
    *vss_kb = 0;
    
    while (fgets(line, sizeof(line), status)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %lu kB", rss_kb);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line, "VmSize: %lu kB", vss_kb);
        }
    }
    
    fclose(status);
}

// Get network statistics from /proc/net/dev
void get_network_stats(uint64_t *bytes_rx, uint64_t *bytes_tx) {
    FILE *netdev = fopen("/proc/net/dev", "r");
    if (!netdev) {
        *bytes_rx = 0;
        *bytes_tx = 0;
        return;
    }
    
    char line[512];
    *bytes_rx = 0;
    *bytes_tx = 0;
    
    // Skip header lines
    fgets(line, sizeof(line), netdev);
    fgets(line, sizeof(line), netdev);
    
    while (fgets(line, sizeof(line), netdev)) {
        char interface[32];
        uint64_t rx_bytes, tx_bytes;
        uint64_t rx_packets, tx_packets;
        uint64_t rx_errs, tx_errs;
        uint64_t rx_drop, tx_drop;
        uint64_t rx_fifo, tx_fifo;
        uint64_t rx_frame, tx_colls;
        uint64_t rx_compressed, tx_carrier;
        uint64_t rx_multicast, tx_compressed;
        
        if (sscanf(line, "%31[^:]: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                   interface, &rx_bytes, &rx_packets, &rx_errs, &rx_drop, &rx_fifo, &rx_frame, &rx_compressed, &rx_multicast,
                   &tx_bytes, &tx_packets, &tx_errs, &tx_drop, &tx_fifo, &tx_colls, &tx_carrier, &tx_compressed) == 17) {
            
            // Skip loopback interface
            if (strncmp(interface, "lo", 2) != 0) {
                *bytes_rx += rx_bytes;
                *bytes_tx += tx_bytes;
            }
        }
    }
    
    fclose(netdev);
}

// Get I/O statistics from /proc/self/io
void get_io_stats(uint64_t *read_bytes, uint64_t *write_bytes) {
    FILE *io = fopen("/proc/self/io", "r");
    if (!io) {
        *read_bytes = 0;
        *write_bytes = 0;
        return;
    }
    
    char line[256];
    *read_bytes = 0;
    *write_bytes = 0;
    
    while (fgets(line, sizeof(line), io)) {
        if (strncmp(line, "read_bytes:", 11) == 0) {
            sscanf(line, "read_bytes: %lu", read_bytes);
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            sscanf(line, "write_bytes: %lu", write_bytes);
        }
    }
    
    fclose(io);
}

// Initialize baseline measurements
void initialize_baseline() {
    if (baseline.initialized) return;
    
    baseline.baseline_cpu = get_cpu_usage();
    get_memory_usage(&baseline.baseline_memory, NULL);
    gettimeofday(&baseline.baseline_time, NULL);
    baseline.initialized = 1;
    
    LOG("[PROF] Baseline established - CPU: %.1f%%, Memory: %lu KB", 
        baseline.baseline_cpu, baseline.baseline_memory);
}

// Collect performance sample
void collect_performance_sample() {
    pthread_mutex_lock(&stats.perf_mutex);
    
    if (stats.sample_count >= stats.max_samples) {
        pthread_mutex_unlock(&stats.perf_mutex);
        return;
    }
    
    perf_sample_t *sample = &stats.samples[stats.sample_count];
    gettimeofday(&sample->timestamp, NULL);
    
    // CPU profiling
    if (prof_cfg.enable_cpu_profiling) {
        sample->cpu_usage_percent = get_cpu_usage();
    }
    
    // Memory profiling
    if (prof_cfg.enable_memory_profiling) {
        get_memory_usage(&sample->memory_rss_kb, &sample->memory_vss_kb);
    }
    
    // Network profiling
    if (prof_cfg.enable_network_profiling) {
        get_network_stats(&sample->network_bytes_rx, &sample->network_bytes_tx);
    }
    
    // I/O profiling
    if (prof_cfg.enable_io_profiling) {
        get_io_stats(&sample->io_reads, &sample->io_writes);
    }
    
    // Calculate throughput (simplified - would need actual packet counting)
    static uint64_t last_bytes_rx = 0;
    static struct timeval last_sample_time = {0, 0};
    
    if (last_sample_time.tv_sec > 0) {
        double time_diff = ((sample->timestamp.tv_sec - last_sample_time.tv_sec) +
                           (sample->timestamp.tv_usec - last_sample_time.tv_usec) / 1000000.0);
        
        if (time_diff > 0 && sample->network_bytes_rx >= last_bytes_rx) {
            uint64_t bytes_diff = sample->network_bytes_rx - last_bytes_rx;
            sample->throughput_mbps = (bytes_diff * 8.0) / (time_diff * 1000000.0);
        }
    }
    
    last_bytes_rx = sample->network_bytes_rx;
    last_sample_time = sample->timestamp;
    
    // Placeholder for latency measurement (would need integration with actual packet processing)
    sample->latency_us = 0.0;
    sample->packets_processed = 0;
    sample->errors_detected = 0;
    
    stats.sample_count++;
    pthread_mutex_unlock(&stats.perf_mutex);
    
    if (prof_cfg.verbose) {
        LOG("[PROF] Sample %d - CPU: %.1f%%, Memory: %lu KB, Throughput: %.2f Mbps",
            stats.sample_count, sample->cpu_usage_percent, sample->memory_rss_kb, sample->throughput_mbps);
    }
}

// Calculate performance trends
void calculate_trends() {
    if (stats.sample_count < 2) return;
    
    pthread_mutex_lock(&stats.perf_mutex);
    
    // Simple linear trend calculation
    double cpu_sum_x = 0, cpu_sum_y = 0, cpu_sum_xy = 0, cpu_sum_x2 = 0;
    double mem_sum_x = 0, mem_sum_y = 0, mem_sum_xy = 0, mem_sum_x2 = 0;
    double tp_sum_x = 0, tp_sum_y = 0, tp_sum_xy = 0, tp_sum_x2 = 0;
    
    for (int i = 0; i < stats.sample_count; i++) {
        double x = i;
        double cpu_y = stats.samples[i].cpu_usage_percent;
        double mem_y = stats.samples[i].memory_rss_kb;
        double tp_y = stats.samples[i].throughput_mbps;
        
        cpu_sum_x += x;
        cpu_sum_y += cpu_y;
        cpu_sum_xy += x * cpu_y;
        cpu_sum_x2 += x * x;
        
        mem_sum_x += x;
        mem_sum_y += mem_y;
        mem_sum_xy += x * mem_y;
        mem_sum_x2 += x * x;
        
        tp_sum_x += x;
        tp_sum_y += tp_y;
        tp_sum_xy += x * tp_y;
        tp_sum_x2 += x * x;
    }
    
    int n = stats.sample_count;
    double denom = n * cpu_sum_x2 - cpu_sum_x * cpu_sum_x;
    
    if (denom != 0) {
        stats.cpu_trend = (n * cpu_sum_xy - cpu_sum_x * cpu_sum_y) / denom;
        stats.memory_trend = (n * mem_sum_xy - mem_sum_x * mem_sum_y) / denom;
        stats.throughput_trend = (n * tp_sum_xy - tp_sum_x * tp_sum_y) / denom;
    }
    
    pthread_mutex_unlock(&stats.perf_mutex);
}

// Analyze performance data
void analyze_performance() {
    if (stats.sample_count == 0) return;
    
    pthread_mutex_lock(&stats.perf_mutex);
    
    // Calculate aggregate statistics
    double cpu_sum = 0, throughput_sum = 0, latency_sum = 0;
    stats.peak_cpu_usage = 0;
    stats.peak_memory_kb = 0;
    stats.peak_throughput_mbps = 0;
    stats.min_latency_us = 0;
    stats.max_latency_us = 0;
    
    for (int i = 0; i < stats.sample_count; i++) {
        perf_sample_t *sample = &stats.samples[i];
        
        // CPU statistics
        cpu_sum += sample->cpu_usage_percent;
        if (sample->cpu_usage_percent > stats.peak_cpu_usage) {
            stats.peak_cpu_usage = sample->cpu_usage_percent;
        }
        
        // Memory statistics
        if (sample->memory_rss_kb > stats.peak_memory_kb) {
            stats.peak_memory_kb = sample->memory_rss_kb;
        }
        
        // Throughput statistics
        throughput_sum += sample->throughput_mbps;
        if (sample->throughput_mbps > stats.peak_throughput_mbps) {
            stats.peak_throughput_mbps = sample->throughput_mbps;
        }
        
        // Latency statistics (if available)
        if (sample->latency_us > 0) {
            latency_sum += sample->latency_us;
            if (stats.min_latency_us == 0 || sample->latency_us < stats.min_latency_us) {
                stats.min_latency_us = sample->latency_us;
            }
            if (sample->latency_us > stats.max_latency_us) {
                stats.max_latency_us = sample->latency_us;
            }
        }
        
        stats.total_packets += sample->packets_processed;
        stats.total_errors += sample->errors_detected;
    }
    
    stats.avg_cpu_usage = cpu_sum / stats.sample_count;
    stats.avg_throughput_mbps = throughput_sum / stats.sample_count;
    stats.avg_latency_us = (latency_sum > 0) ? latency_sum / stats.sample_count : 0;
    
    pthread_mutex_unlock(&stats.perf_mutex);
    
    // Calculate trends
    calculate_trends();
}

char* generate_performance_config() {
    char *config = malloc(1024);
    if (!config) {
        LOG("[PROF] Failed to allocate memory for config");
        return NULL;
    }

    snprintf(config, 1024,
        "{\n"
        "  \"connection\": {\n"
        "    \"performance\": {\n"
        "      \"optimizeForThroughput\": %s,\n"
        "      \"optimizeForLatency\": %s,\n"
        "      \"enableProfiling\": true,\n"
        "      \"sampleInterval\": %d,\n"
        "      \"bufferSize\": \"adaptive\"\n"
        "    },\n"
        "    \"monitoring\": {\n"
        "      \"cpuProfiling\": %s,\n"
        "      \"memoryProfiling\": %s,\n"
        "      \"networkProfiling\": %s,\n"
        "      \"ioProfiling\": %s\n"
        "    }\n"
        "  },\n"
        "  \"payload\": {\n"
        "    \"%s\": %s\n"
        "  }\n"
        "}",
        strcmp(prof_cfg.profile_mode, "throughput") == 0 ? "true" : "false",
        strcmp(prof_cfg.profile_mode, "latency") == 0 ? "true" : "false",
        prof_cfg.sample_interval_ms,
        prof_cfg.enable_cpu_profiling ? "true" : "false",
        prof_cfg.enable_memory_profiling ? "true" : "false",
        prof_cfg.enable_network_profiling ? "true" : "false",
        prof_cfg.enable_io_profiling ? "true" : "false",
        prof_cfg.payload_type,
        strcmp(prof_cfg.payload_type, "video") == 0 ? 
            "{ \"width\": 1920, \"height\": 1080, \"fps\": 30, \"pixelFormat\": \"yuv422p10le\" }" :
        strcmp(prof_cfg.payload_type, "audio") == 0 ?
            "{ \"channels\": 2, \"sampleRate\": 48000, \"format\": \"pcm_s16le\" }" :
            "{}");

    return config;
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    if (stats.sample_count > 0) {
        perf_sample_t *latest = &stats.samples[stats.sample_count - 1];
        printf("\r[PROF] Progress: %.1fs | Samples: %d | CPU: %.1f%% | Memory: %lu KB | Throughput: %.2f Mbps", 
               elapsed, stats.sample_count, latest->cpu_usage_percent, 
               latest->memory_rss_kb, latest->throughput_mbps);
    } else {
        printf("\r[PROF] Progress: %.1fs | Samples: %d", elapsed, stats.sample_count);
    }
    
    fflush(stdout);
}

void save_performance_results() {
    // Save detailed results
    if (strlen(prof_cfg.output_file) > 0) {
        FILE *f = fopen(prof_cfg.output_file, "w");
        if (!f) {
            LOG("[PROF] Failed to open output file: %s", prof_cfg.output_file);
        } else {
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                                (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
            
            fprintf(f, "# Performance Profiling Test Results\n");
            fprintf(f, "Profile Mode: %s\n", prof_cfg.profile_mode);
            fprintf(f, "Payload Type: %s\n", prof_cfg.payload_type);
            fprintf(f, "Test Duration: %.2f seconds\n", total_time);
            fprintf(f, "Sample Interval: %d ms\n", prof_cfg.sample_interval_ms);
            fprintf(f, "Total Samples: %d\n", stats.sample_count);
            
            fprintf(f, "\nPerformance Summary:\n");
            fprintf(f, "Average CPU Usage: %.2f%%\n", stats.avg_cpu_usage);
            fprintf(f, "Peak CPU Usage: %.2f%%\n", stats.peak_cpu_usage);
            fprintf(f, "Peak Memory Usage: %.2f MB\n", stats.peak_memory_kb / 1024.0);
            fprintf(f, "Average Throughput: %.2f Mbps\n", stats.avg_throughput_mbps);
            fprintf(f, "Peak Throughput: %.2f Mbps\n", stats.peak_throughput_mbps);
            
            if (stats.avg_latency_us > 0) {
                fprintf(f, "Average Latency: %.2f μs\n", stats.avg_latency_us);
                fprintf(f, "Min Latency: %.2f μs\n", stats.min_latency_us);
                fprintf(f, "Max Latency: %.2f μs\n", stats.max_latency_us);
            }
            
            fprintf(f, "\nPerformance Trends:\n");
            fprintf(f, "CPU Trend: %.4f %%/sample\n", stats.cpu_trend);
            fprintf(f, "Memory Trend: %.4f KB/sample\n", stats.memory_trend);
            fprintf(f, "Throughput Trend: %.4f Mbps/sample\n", stats.throughput_trend);
            
            fclose(f);
            LOG("[PROF] Test results saved to: %s", prof_cfg.output_file);
        }
    }
    
    // Save CSV data
    if (strlen(prof_cfg.csv_output) > 0) {
        FILE *csv = fopen(prof_cfg.csv_output, "w");
        if (!csv) {
            LOG("[PROF] Failed to open CSV file: %s", prof_cfg.csv_output);
        } else {
            // CSV header
            fprintf(csv, "timestamp,cpu_usage,memory_rss_kb,memory_vss_kb,network_rx,network_tx,io_reads,io_writes,throughput_mbps,latency_us,packets,errors\n");
            
            // CSV data
            pthread_mutex_lock(&stats.perf_mutex);
            for (int i = 0; i < stats.sample_count; i++) {
                perf_sample_t *sample = &stats.samples[i];
                fprintf(csv, "%ld.%06ld,%.2f,%lu,%lu,%lu,%lu,%lu,%lu,%.2f,%.2f,%lu,%lu\n",
                       sample->timestamp.tv_sec, sample->timestamp.tv_usec,
                       sample->cpu_usage_percent, sample->memory_rss_kb, sample->memory_vss_kb,
                       sample->network_bytes_rx, sample->network_bytes_tx,
                       sample->io_reads, sample->io_writes,
                       sample->throughput_mbps, sample->latency_us,
                       sample->packets_processed, sample->errors_detected);
            }
            pthread_mutex_unlock(&stats.perf_mutex);
            
            fclose(csv);
            LOG("[PROF] CSV data saved to: %s", prof_cfg.csv_output);
        }
    }
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[PROF] Starting Performance Profiling Test");
    LOG("[PROF] Mode: %s, Payload: %s, Duration: %d seconds", 
        prof_cfg.profile_mode, prof_cfg.payload_type, prof_cfg.test_duration_sec);
    LOG("[PROF] Sample Interval: %d ms, Profiling: %s%s%s%s", 
        prof_cfg.sample_interval_ms,
        prof_cfg.enable_cpu_profiling ? "CPU " : "",
        prof_cfg.enable_memory_profiling ? "Memory " : "",
        prof_cfg.enable_io_profiling ? "I/O " : "",
        prof_cfg.enable_network_profiling ? "Network " : "");

    // Initialize mutex and allocate samples array
    pthread_mutex_init(&stats.perf_mutex, NULL);
    stats.max_samples = (prof_cfg.test_duration_sec * 1000) / prof_cfg.sample_interval_ms + 100;
    stats.samples = malloc(stats.max_samples * sizeof(perf_sample_t));
    
    if (!stats.samples) {
        LOG("[PROF] Failed to allocate memory for samples");
        exit(EXIT_FAILURE);
    }

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate performance-aware connection configuration
    conn_cfg = generate_performance_config();
    if (!conn_cfg) {
        free(client_cfg);
        free(stats.samples);
        exit(EXIT_FAILURE);
    }

    LOG("[PROF] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[PROF] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[PROF] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Initialize statistics and baseline
    gettimeofday(&stats.start_time, NULL);
    initialize_baseline();
    
    LOG("[PROF] Starting performance profiling for %d seconds...", prof_cfg.test_duration_sec);

    // Main profiling loop
    struct timeval test_end, last_sample;
    gettimeofday(&test_end, NULL);
    gettimeofday(&last_sample, NULL);
    test_end.tv_sec += prof_cfg.test_duration_sec;

    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        if (shutdown_flag == SHUTDOWN_REQUESTED) {
            LOG("[PROF] Graceful shutdown requested");
            break;
        }
        
        // Check if it's time to collect a sample
        double sample_elapsed = ((now.tv_sec - last_sample.tv_sec) * 1000.0) +
                               ((now.tv_usec - last_sample.tv_usec) / 1000.0);
        
        if (sample_elapsed >= prof_cfg.sample_interval_ms) {
            collect_performance_sample();
            last_sample = now;
        }
        
        // Regular mesh operations
        MeshBuffer *buf;
        err = mesh_get_buffer_timeout(connection, &buf, 100);
        
        if (err == 0 && buf) {
            // Process buffer (this contributes to performance measurements)
            mesh_put_buffer(&buf);
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
        
        usleep(50000); // 50ms
    }
    
    printf("\n");
    
    // Analyze collected performance data
    analyze_performance();
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[PROF] Profiling completed in %.2f seconds", total_time);
    LOG("[PROF] Collected %d performance samples", stats.sample_count);
    LOG("[PROF] Average CPU usage: %.2f%% (peak: %.2f%%)", stats.avg_cpu_usage, stats.peak_cpu_usage);
    LOG("[PROF] Peak memory usage: %.2f MB", stats.peak_memory_kb / 1024.0);
    LOG("[PROF] Average throughput: %.2f Mbps (peak: %.2f Mbps)", stats.avg_throughput_mbps, stats.peak_throughput_mbps);
    
    if (prof_cfg.detailed_analysis) {
        LOG("[PROF] Performance trends - CPU: %.4f%%/sample, Memory: %.4f KB/sample, Throughput: %.4f Mbps/sample",
            stats.cpu_trend, stats.memory_trend, stats.throughput_trend);
        
        // Performance analysis
        if (stats.cpu_trend > 0.1) {
            LOG("[PROF] WARNING: CPU usage trend is increasing significantly");
        }
        if (stats.memory_trend > 100) {
            LOG("[PROF] WARNING: Memory usage trend is increasing significantly");
        }
        if (stats.throughput_trend < -0.1) {
            LOG("[PROF] WARNING: Throughput trend is decreasing");
        }
    }
    
    // Save results to files if specified
    save_performance_results();

safe_exit:
    LOG("[PROF] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[PROF] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    
    pthread_mutex_destroy(&stats.perf_mutex);
    free(stats.samples);
    free(client_cfg);
    free(conn_cfg);
    return err;
}
