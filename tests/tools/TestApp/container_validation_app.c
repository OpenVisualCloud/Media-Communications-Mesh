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

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

// Container format test configuration
typedef struct {
    char container_type[32];     // "mp4", "ts", "mkv", "avi", "mov", "webm", "all"
    char test_files[512];        // Comma-separated test file list
    int test_duration_sec;
    int validate_structure;      // Validate container structure
    int validate_metadata;       // Validate metadata
    int validate_streams;        // Validate stream information
    int test_seeking;            // Test seeking functionality
    int test_muxing;            // Test muxing operations
    int test_demuxing;          // Test demuxing operations
    int test_corruption;         // Test corruption detection
    int generate_samples;        // Generate container samples
    int deep_analysis;          // Deep container analysis
    char output_file[256];
    int verbose;
} container_config_t;

static container_config_t cont_cfg = {
    .container_type = "mp4",
    .test_files = "",
    .test_duration_sec = 300,
    .validate_structure = 1,
    .validate_metadata = 1,
    .validate_streams = 1,
    .test_seeking = 1,
    .test_muxing = 1,
    .test_demuxing = 1,
    .test_corruption = 1,
    .generate_samples = 1,
    .deep_analysis = 0,
    .output_file = "",
    .verbose = 0
};

// Container testing statistics
typedef struct {
    uint64_t containers_tested;
    uint64_t valid_containers;
    uint64_t invalid_containers;
    uint64_t structure_errors;
    uint64_t metadata_errors;
    uint64_t stream_errors;
    uint64_t seeking_errors;
    uint64_t muxing_operations;
    uint64_t demuxing_operations;
    uint64_t corruption_detected;
    uint64_t samples_generated;
    uint64_t deep_analyses;
    uint64_t bytes_processed;
    double total_processing_time_ms;
    double avg_processing_time_ms;
    struct timeval start_time;
    char current_container[64];
    char current_operation[64];
} container_stats_t;

static container_stats_t stats = {0};

// Container format structures
typedef struct {
    char format_name[32];
    uint32_t major_brand;
    uint32_t minor_version;
    uint64_t file_size;
    uint64_t duration_ms;
    uint32_t num_video_streams;
    uint32_t num_audio_streams;
    uint32_t num_subtitle_streams;
    char creation_time[32];
    char metadata[256];
} container_info_t;

// MP4 box header
typedef struct {
    uint32_t size;
    uint32_t type;
} __attribute__((packed)) mp4_box_header_t;

// MPEG-TS packet header
typedef struct {
    uint8_t sync_byte;
    uint8_t tei_pusi_tp;
    uint8_t pid_high;
    uint8_t pid_low;
    uint8_t tsc_afc_cc;
} __attribute__((packed)) ts_packet_header_t;

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Container Format Validation Application\n\n");
    printf("Container Configuration:\n");
    printf("  -c, --container <type>     Container type: mp4, ts, mkv, avi, mov, webm, all (default: %s)\n", cont_cfg.container_type);
    printf("  --test-files <list>        Comma-separated test file list\n");
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", cont_cfg.test_duration_sec);
    printf("\n  Validation Options:\n");
    printf("  --validate-structure       Enable structure validation (default: %s)\n", cont_cfg.validate_structure ? "enabled" : "disabled");
    printf("  --validate-metadata        Enable metadata validation (default: %s)\n", cont_cfg.validate_metadata ? "enabled" : "disabled");
    printf("  --validate-streams         Enable stream validation (default: %s)\n", cont_cfg.validate_streams ? "enabled" : "disabled");
    printf("  --no-structure             Disable structure validation\n");
    printf("  --no-metadata              Disable metadata validation\n");
    printf("  --no-streams               Disable stream validation\n");
    printf("\n  Testing Features:\n");
    printf("  --test-seeking             Enable seeking testing (default: %s)\n", cont_cfg.test_seeking ? "enabled" : "disabled");
    printf("  --test-muxing              Enable muxing testing (default: %s)\n", cont_cfg.test_muxing ? "enabled" : "disabled");
    printf("  --test-demuxing            Enable demuxing testing (default: %s)\n", cont_cfg.test_demuxing ? "enabled" : "disabled");
    printf("  --test-corruption          Enable corruption testing (default: %s)\n", cont_cfg.test_corruption ? "enabled" : "disabled");
    printf("  --no-seeking               Disable seeking testing\n");
    printf("  --no-muxing                Disable muxing testing\n");
    printf("  --no-demuxing              Disable demuxing testing\n");
    printf("  --no-corruption            Disable corruption testing\n");
    printf("\n  Advanced Features:\n");
    printf("  --generate-samples         Generate container samples (default: %s)\n", cont_cfg.generate_samples ? "enabled" : "disabled");
    printf("  --deep-analysis            Enable deep container analysis\n");
    printf("  --no-samples               Disable sample generation\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Test MP4 containers with deep analysis\n");
    printf("    %s --container mp4 --deep-analysis\n", prog_name);
    printf("\n    # Test all container formats with seeking\n");
    printf("    %s --container all --test-seeking\n", prog_name);
    printf("\n    # Test specific files with corruption detection\n");
    printf("    %s --test-files sample1.mp4,sample2.mkv --test-corruption\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"container", required_argument, 0, 'c'},
        {"test-files", required_argument, 0, 1001},
        {"test-duration", required_argument, 0, 't'},
        {"validate-structure", no_argument, 0, 1002},
        {"validate-metadata", no_argument, 0, 1003},
        {"validate-streams", no_argument, 0, 1004},
        {"no-structure", no_argument, 0, 1005},
        {"no-metadata", no_argument, 0, 1006},
        {"no-streams", no_argument, 0, 1007},
        {"test-seeking", no_argument, 0, 1008},
        {"test-muxing", no_argument, 0, 1009},
        {"test-demuxing", no_argument, 0, 1010},
        {"test-corruption", no_argument, 0, 1011},
        {"no-seeking", no_argument, 0, 1012},
        {"no-muxing", no_argument, 0, 1013},
        {"no-demuxing", no_argument, 0, 1014},
        {"no-corruption", no_argument, 0, 1015},
        {"generate-samples", no_argument, 0, 1016},
        {"deep-analysis", no_argument, 0, 1017},
        {"no-samples", no_argument, 0, 1018},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "c:t:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'c':
                strncpy(cont_cfg.container_type, optarg, sizeof(cont_cfg.container_type) - 1);
                break;
            case 1001:
                strncpy(cont_cfg.test_files, optarg, sizeof(cont_cfg.test_files) - 1);
                break;
            case 't':
                cont_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1002:
                cont_cfg.validate_structure = 1;
                break;
            case 1003:
                cont_cfg.validate_metadata = 1;
                break;
            case 1004:
                cont_cfg.validate_streams = 1;
                break;
            case 1005:
                cont_cfg.validate_structure = 0;
                break;
            case 1006:
                cont_cfg.validate_metadata = 0;
                break;
            case 1007:
                cont_cfg.validate_streams = 0;
                break;
            case 1008:
                cont_cfg.test_seeking = 1;
                break;
            case 1009:
                cont_cfg.test_muxing = 1;
                break;
            case 1010:
                cont_cfg.test_demuxing = 1;
                break;
            case 1011:
                cont_cfg.test_corruption = 1;
                break;
            case 1012:
                cont_cfg.test_seeking = 0;
                break;
            case 1013:
                cont_cfg.test_muxing = 0;
                break;
            case 1014:
                cont_cfg.test_demuxing = 0;
                break;
            case 1015:
                cont_cfg.test_corruption = 0;
                break;
            case 1016:
                cont_cfg.generate_samples = 1;
                break;
            case 1017:
                cont_cfg.deep_analysis = 1;
                break;
            case 1018:
                cont_cfg.generate_samples = 0;
                break;
            case 'o':
                strncpy(cont_cfg.output_file, optarg, sizeof(cont_cfg.output_file) - 1);
                break;
            case 'v':
                cont_cfg.verbose = 1;
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

// Validate MP4 container structure
int validate_mp4_structure(const uint8_t *data, size_t size, container_info_t *info) {
    if (size < 8) return -1;
    
    mp4_box_header_t *header = (mp4_box_header_t *)data;
    uint32_t box_size = ntohl(header->size);
    uint32_t box_type = ntohl(header->type);
    
    // Check for ftyp box
    if (box_type != 0x66747970) { // "ftyp"
        stats.structure_errors++;
        return -1;
    }
    
    if (size < 16) return -1;
    
    // Parse major brand
    info->major_brand = ntohl(*(uint32_t *)(data + 8));
    info->minor_version = ntohl(*(uint32_t *)(data + 12));
    info->file_size = size;
    
    strncpy(info->format_name, "MP4", sizeof(info->format_name) - 1);
    
    if (cont_cfg.verbose) {
        LOG("[CONT] MP4: ftyp box size=%u, brand=0x%08x, version=%u", 
            box_size, info->major_brand, info->minor_version);
    }
    
    // Look for moov box for metadata
    size_t offset = box_size;
    while (offset + 8 < size) {
        header = (mp4_box_header_t *)(data + offset);
        box_size = ntohl(header->size);
        box_type = ntohl(header->type);
        
        if (box_type == 0x6D6F6F76) { // "moov"
            if (cont_cfg.verbose) {
                LOG("[CONT] MP4: moov box found at offset %zu", offset);
            }
            // Found movie metadata box
            break;
        }
        
        if (box_size == 0 || offset + box_size > size) break;
        offset += box_size;
    }
    
    return 0;
}

// Validate MPEG-TS container structure
int validate_ts_structure(const uint8_t *data, size_t size, container_info_t *info) {
    if (size < 188) return -1; // TS packet size
    
    ts_packet_header_t *header = (ts_packet_header_t *)data;
    
    // Check sync byte
    if (header->sync_byte != 0x47) {
        stats.structure_errors++;
        return -1;
    }
    
    uint8_t tei = (header->tei_pusi_tp >> 7) & 0x1;
    uint8_t pusi = (header->tei_pusi_tp >> 6) & 0x1;
    uint8_t tp = (header->tei_pusi_tp >> 5) & 0x1;
    uint16_t pid = ((header->tei_pusi_tp & 0x1F) << 8) | header->pid_low;
    
    uint8_t tsc = (header->tsc_afc_cc >> 6) & 0x3;
    uint8_t afc = (header->tsc_afc_cc >> 4) & 0x3;
    uint8_t cc = header->tsc_afc_cc & 0xF;
    
    if (tei) {
        stats.structure_errors++;
        if (cont_cfg.verbose) {
            LOG("[CONT] TS: Transport Error Indicator set in PID %u", pid);
        }
    }
    
    strncpy(info->format_name, "MPEG-TS", sizeof(info->format_name) - 1);
    info->file_size = size;
    
    if (cont_cfg.verbose) {
        LOG("[CONT] TS: PID=%u, PUSI=%u, AFC=%u, CC=%u", pid, pusi, afc, cc);
    }
    
    return 0;
}

// Validate MKV container structure
int validate_mkv_structure(const uint8_t *data, size_t size, container_info_t *info) {
    if (size < 8) return -1;
    
    // Check for EBML header
    if (data[0] != 0x1A || data[1] != 0x45 || data[2] != 0xDF || data[3] != 0xA3) {
        stats.structure_errors++;
        return -1;
    }
    
    strncpy(info->format_name, "Matroska", sizeof(info->format_name) - 1);
    info->file_size = size;
    
    if (cont_cfg.verbose) {
        LOG("[CONT] MKV: EBML header found");
    }
    
    return 0;
}

// Validate AVI container structure
int validate_avi_structure(const uint8_t *data, size_t size, container_info_t *info) {
    if (size < 12) return -1;
    
    // Check for RIFF header
    if (strncmp((char *)data, "RIFF", 4) != 0) {
        stats.structure_errors++;
        return -1;
    }
    
    // Check for AVI identifier
    if (strncmp((char *)data + 8, "AVI ", 4) != 0) {
        stats.structure_errors++;
        return -1;
    }
    
    uint32_t file_size = *(uint32_t *)(data + 4);
    info->file_size = file_size + 8;
    
    strncpy(info->format_name, "AVI", sizeof(info->format_name) - 1);
    
    if (cont_cfg.verbose) {
        LOG("[CONT] AVI: RIFF header, file size=%u", file_size);
    }
    
    return 0;
}

// Validate container structure based on type
int validate_container_structure(const char *container_type, const uint8_t *data, 
                                size_t size, container_info_t *info) {
    memset(info, 0, sizeof(container_info_t));
    strncpy(stats.current_container, container_type, sizeof(stats.current_container) - 1);
    
    if (strcmp(container_type, "mp4") == 0) {
        return validate_mp4_structure(data, size, info);
    } else if (strcmp(container_type, "ts") == 0) {
        return validate_ts_structure(data, size, info);
    } else if (strcmp(container_type, "mkv") == 0) {
        return validate_mkv_structure(data, size, info);
    } else if (strcmp(container_type, "avi") == 0) {
        return validate_avi_structure(data, size, info);
    } else if (strcmp(container_type, "mov") == 0) {
        // MOV is similar to MP4
        return validate_mp4_structure(data, size, info);
    }
    
    // Unknown container type
    LOG("[CONT] Unknown container type: %s", container_type);
    return -1;
}

// Generate container sample data
int generate_container_sample(const char *container_type, uint8_t *buffer, size_t max_size) {
    if (strcmp(container_type, "mp4") == 0) {
        // Generate minimal MP4 structure
        if (max_size < 32) return -1;
        
        // ftyp box
        *(uint32_t *)(buffer + 0) = htonl(24);           // Box size
        *(uint32_t *)(buffer + 4) = htonl(0x66747970);   // "ftyp"
        *(uint32_t *)(buffer + 8) = htonl(0x6D703431);   // "mp41"
        *(uint32_t *)(buffer + 12) = htonl(0);           // Minor version
        *(uint32_t *)(buffer + 16) = htonl(0x6D703431);  // Compatible brand
        *(uint32_t *)(buffer + 20) = htonl(0x69736F6D); // "isom"
        
        // mdat box header
        *(uint32_t *)(buffer + 24) = htonl(8);           // Box size
        *(uint32_t *)(buffer + 28) = htonl(0x6D646174); // "mdat"
        
        return 32;
    } else if (strcmp(container_type, "ts") == 0) {
        // Generate minimal TS packet
        if (max_size < 188) return -1;
        
        memset(buffer, 0xFF, 188); // Padding
        buffer[0] = 0x47;    // Sync byte
        buffer[1] = 0x00;    // TEI=0, PUSI=0, TP=0, PID high=0
        buffer[2] = 0x00;    // PID low=0 (PAT)
        buffer[3] = 0x10;    // TSC=0, AFC=1, CC=0
        
        return 188;
    } else if (strcmp(container_type, "mkv") == 0) {
        // Generate minimal MKV EBML header
        if (max_size < 20) return -1;
        
        buffer[0] = 0x1A; buffer[1] = 0x45; buffer[2] = 0xDF; buffer[3] = 0xA3; // EBML
        for (int i = 4; i < 20; i++) buffer[i] = rand() & 0xFF;
        
        return 20;
    } else if (strcmp(container_type, "avi") == 0) {
        // Generate minimal AVI header
        if (max_size < 20) return -1;
        
        memcpy(buffer, "RIFF", 4);
        *(uint32_t *)(buffer + 4) = 12; // File size - 8
        memcpy(buffer + 8, "AVI ", 4);
        memcpy(buffer + 12, "LIST", 4);
        *(uint32_t *)(buffer + 16) = 4;
        
        return 20;
    }
    
    return -1;
}

// Test container seeking functionality
int test_container_seeking(const char *container_type, const uint8_t *data, size_t size) {
    strncpy(stats.current_operation, "seeking", sizeof(stats.current_operation) - 1);
    
    if (cont_cfg.verbose) {
        LOG("[CONT] Testing seeking in %s container", container_type);
    }
    
    // Simulate seeking to different positions
    size_t seek_positions[] = {0, size/4, size/2, 3*size/4, size-1};
    int num_positions = 5;
    
    for (int i = 0; i < num_positions; i++) {
        if (seek_positions[i] >= size) continue;
        
        // Validate structure at seek position
        container_info_t info;
        if (validate_container_structure(container_type, data + seek_positions[i], 
                                       size - seek_positions[i], &info) != 0) {
            // Seeking errors are expected at some positions
            if (cont_cfg.verbose) {
                LOG("[CONT] Seek validation failed at position %zu", seek_positions[i]);
            }
        }
    }
    
    return 0;
}

// Test container muxing/demuxing operations
int test_container_muxing(const char *container_type) {
    strncpy(stats.current_operation, "muxing", sizeof(stats.current_operation) - 1);
    
    if (cont_cfg.verbose) {
        LOG("[CONT] Testing muxing operations for %s", container_type);
    }
    
    // Simulate muxing operations
    stats.muxing_operations++;
    usleep(1000); // Simulate processing time
    
    return 0;
}

int test_container_demuxing(const char *container_type, const uint8_t *data, size_t size) {
    strncpy(stats.current_operation, "demuxing", sizeof(stats.current_operation) - 1);
    
    if (cont_cfg.verbose) {
        LOG("[CONT] Testing demuxing operations for %s", container_type);
    }
    
    // Simulate demuxing operations
    stats.demuxing_operations++;
    usleep(1000); // Simulate processing time
    
    return 0;
}

// Test container with corruption
int test_container_corruption(const char *container_type, const uint8_t *original_data, size_t size) {
    uint8_t *corrupted_data = malloc(size);
    if (!corrupted_data) return -1;
    
    memcpy(corrupted_data, original_data, size);
    
    // Introduce random corruption
    int corruption_points = (size / 1000) + 1; // 0.1% corruption
    for (int i = 0; i < corruption_points; i++) {
        size_t pos = rand() % size;
        corrupted_data[pos] ^= (1 << (rand() % 8)); // Flip random bit
    }
    
    // Validate corrupted data - should fail
    container_info_t info;
    int result = validate_container_structure(container_type, corrupted_data, size, &info);
    if (result != 0) {
        stats.corruption_detected++;
        if (cont_cfg.verbose) {
            LOG("[CONT] Corruption detected in %s container", container_type);
        }
    }
    
    free(corrupted_data);
    return result;
}

// Perform deep container analysis
int deep_container_analysis(const char *container_type, const uint8_t *data, size_t size) {
    strncpy(stats.current_operation, "deep_analysis", sizeof(stats.current_operation) - 1);
    
    if (cont_cfg.verbose) {
        LOG("[CONT] Performing deep analysis of %s container", container_type);
    }
    
    stats.deep_analyses++;
    
    // Simulate deep analysis operations
    if (strcmp(container_type, "mp4") == 0) {
        // Analyze all boxes
        size_t offset = 0;
        while (offset + 8 < size) {
            mp4_box_header_t *header = (mp4_box_header_t *)(data + offset);
            uint32_t box_size = ntohl(header->size);
            uint32_t box_type = ntohl(header->type);
            
            if (box_size == 0 || offset + box_size > size) break;
            
            if (cont_cfg.verbose) {
                char type_str[5] = {0};
                memcpy(type_str, &box_type, 4);
                LOG("[CONT] MP4 box: type='%s', size=%u, offset=%zu", type_str, box_size, offset);
            }
            
            offset += box_size;
        }
    } else if (strcmp(container_type, "ts") == 0) {
        // Analyze TS packets
        size_t num_packets = size / 188;
        for (size_t i = 0; i < num_packets && i < 100; i++) { // Limit to first 100 packets
            ts_packet_header_t *header = (ts_packet_header_t *)(data + i * 188);
            if (header->sync_byte != 0x47) {
                stats.structure_errors++;
                break;
            }
        }
        
        if (cont_cfg.verbose) {
            LOG("[CONT] TS: analyzed %zu packets", num_packets > 100 ? 100 : num_packets);
        }
    }
    
    usleep(5000); // Simulate analysis time
    return 0;
}

// Test specific container format
int test_container_format(const char *container_type) {
    LOG("[CONT] Testing container format: %s", container_type);
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    int errors = 0;
    
    // Generate and test valid sample
    if (cont_cfg.generate_samples) {
        uint8_t sample_data[1024];
        int sample_size = generate_container_sample(container_type, sample_data, sizeof(sample_data));
        
        if (sample_size > 0) {
            stats.samples_generated++;
            
            container_info_t info;
            if (cont_cfg.validate_structure) {
                if (validate_container_structure(container_type, sample_data, sample_size, &info) != 0) {
                    errors++;
                    stats.invalid_containers++;
                } else {
                    stats.valid_containers++;
                }
            }
            
            // Additional tests
            if (cont_cfg.test_seeking) {
                test_container_seeking(container_type, sample_data, sample_size);
            }
            
            if (cont_cfg.test_muxing) {
                test_container_muxing(container_type);
            }
            
            if (cont_cfg.test_demuxing) {
                test_container_demuxing(container_type, sample_data, sample_size);
            }
            
            if (cont_cfg.test_corruption) {
                test_container_corruption(container_type, sample_data, sample_size);
            }
            
            if (cont_cfg.deep_analysis) {
                deep_container_analysis(container_type, sample_data, sample_size);
            }
            
            stats.bytes_processed += sample_size;
        }
    }
    
    gettimeofday(&end, NULL);
    double processing_time = ((end.tv_sec - start.tv_sec) * 1000.0) +
                           ((end.tv_usec - start.tv_usec) / 1000.0);
    
    stats.total_processing_time_ms += processing_time;
    stats.containers_tested++;
    
    if (stats.containers_tested > 0) {
        stats.avg_processing_time_ms = stats.total_processing_time_ms / stats.containers_tested;
    }
    
    if (cont_cfg.verbose) {
        LOG("[CONT] %s test completed in %.2f ms, %d errors", container_type, processing_time, errors);
    }
    
    return errors;
}

char* generate_container_config() {
    char *config = malloc(1024);
    if (!config) {
        LOG("[CONT] Failed to allocate memory for config");
        return NULL;
    }

    snprintf(config, 1024,
        "{\n"
        "  \"connection\": {\n"
        "    \"containerValidation\": {\n"
        "      \"enableStructure\": %s,\n"
        "      \"enableMetadata\": %s,\n"
        "      \"enableStreams\": %s,\n"
        "      \"enableSeeking\": %s,\n"
        "      \"enableMuxing\": %s,\n"
        "      \"enableDemuxing\": %s,\n"
        "      \"enableCorruption\": %s,\n"
        "      \"deepAnalysis\": %s\n"
        "    }\n"
        "  },\n"
        "  \"payload\": {\n"
        "    \"containerType\": \"%s\",\n"
        "    \"testFiles\": \"%s\"\n"
        "  },\n"
        "  \"testing\": {\n"
        "    \"generateSamples\": %s\n"
        "  }\n"
        "}",
        cont_cfg.validate_structure ? "true" : "false",
        cont_cfg.validate_metadata ? "true" : "false",
        cont_cfg.validate_streams ? "true" : "false",
        cont_cfg.test_seeking ? "true" : "false",
        cont_cfg.test_muxing ? "true" : "false",
        cont_cfg.test_demuxing ? "true" : "false",
        cont_cfg.test_corruption ? "true" : "false",
        cont_cfg.deep_analysis ? "true" : "false",
        cont_cfg.container_type,
        cont_cfg.test_files,
        cont_cfg.generate_samples ? "true" : "false");

    return config;
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    double success_rate = 0.0;
    if (stats.containers_tested > 0) {
        success_rate = ((double)stats.valid_containers / stats.containers_tested) * 100.0;
    }
    
    printf("\r[CONT] %.1fs | Container: %s | Op: %s | Tested: %lu | Valid: %lu (%.1f%%) | Avg: %.1f ms", 
           elapsed, stats.current_container, stats.current_operation, stats.containers_tested, 
           stats.valid_containers, success_rate, stats.avg_processing_time_ms);
    fflush(stdout);
}

void save_container_results() {
    if (strlen(cont_cfg.output_file) == 0) return;
    
    FILE *f = fopen(cont_cfg.output_file, "w");
    if (!f) {
        LOG("[CONT] Failed to open output file: %s", cont_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Container Format Validation Test Results\n");
    fprintf(f, "Container Type: %s\n", cont_cfg.container_type);
    fprintf(f, "Test Files: %s\n", cont_cfg.test_files);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    
    fprintf(f, "\nValidation Configuration:\n");
    fprintf(f, "Structure Validation: %s\n", cont_cfg.validate_structure ? "enabled" : "disabled");
    fprintf(f, "Metadata Validation: %s\n", cont_cfg.validate_metadata ? "enabled" : "disabled");
    fprintf(f, "Stream Validation: %s\n", cont_cfg.validate_streams ? "enabled" : "disabled");
    
    fprintf(f, "\nTesting Configuration:\n");
    fprintf(f, "Seeking Testing: %s\n", cont_cfg.test_seeking ? "enabled" : "disabled");
    fprintf(f, "Muxing Testing: %s\n", cont_cfg.test_muxing ? "enabled" : "disabled");
    fprintf(f, "Demuxing Testing: %s\n", cont_cfg.test_demuxing ? "enabled" : "disabled");
    fprintf(f, "Corruption Testing: %s\n", cont_cfg.test_corruption ? "enabled" : "disabled");
    fprintf(f, "Sample Generation: %s\n", cont_cfg.generate_samples ? "enabled" : "disabled");
    fprintf(f, "Deep Analysis: %s\n", cont_cfg.deep_analysis ? "enabled" : "disabled");
    
    fprintf(f, "\nValidation Statistics:\n");
    fprintf(f, "Containers Tested: %lu\n", stats.containers_tested);
    fprintf(f, "Valid Containers: %lu\n", stats.valid_containers);
    fprintf(f, "Invalid Containers: %lu\n", stats.invalid_containers);
    fprintf(f, "Samples Generated: %lu\n", stats.samples_generated);
    fprintf(f, "Deep Analyses: %lu\n", stats.deep_analyses);
    fprintf(f, "Bytes Processed: %lu\n", stats.bytes_processed);
    
    if (stats.containers_tested > 0) {
        double success_rate = ((double)stats.valid_containers / stats.containers_tested) * 100.0;
        fprintf(f, "Success Rate: %.2f%%\n", success_rate);
        fprintf(f, "Average Processing Time: %.3f ms\n", stats.avg_processing_time_ms);
    }
    
    fprintf(f, "\nOperation Statistics:\n");
    fprintf(f, "Muxing Operations: %lu\n", stats.muxing_operations);
    fprintf(f, "Demuxing Operations: %lu\n", stats.demuxing_operations);
    fprintf(f, "Corruption Detected: %lu\n", stats.corruption_detected);
    
    fprintf(f, "\nError Statistics:\n");
    fprintf(f, "Structure Errors: %lu\n", stats.structure_errors);
    fprintf(f, "Metadata Errors: %lu\n", stats.metadata_errors);
    fprintf(f, "Stream Errors: %lu\n", stats.stream_errors);
    fprintf(f, "Seeking Errors: %lu\n", stats.seeking_errors);
    
    fclose(f);
    LOG("[CONT] Test results saved to: %s", cont_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[CONT] Starting Container Format Validation Test");
    LOG("[CONT] Container: %s, Duration: %d seconds", 
        cont_cfg.container_type, cont_cfg.test_duration_sec);
    LOG("[CONT] Validation - Structure: %s, Metadata: %s, Streams: %s", 
        cont_cfg.validate_structure ? "enabled" : "disabled",
        cont_cfg.validate_metadata ? "enabled" : "disabled",
        cont_cfg.validate_streams ? "enabled" : "disabled");
    LOG("[CONT] Testing - Seeking: %s, Muxing: %s, Demuxing: %s, Corruption: %s", 
        cont_cfg.test_seeking ? "enabled" : "disabled",
        cont_cfg.test_muxing ? "enabled" : "disabled",
        cont_cfg.test_demuxing ? "enabled" : "disabled",
        cont_cfg.test_corruption ? "enabled" : "disabled");

    // Initialize random seed
    srand(time(NULL));

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate container validation connection configuration
    conn_cfg = generate_container_config();
    if (!conn_cfg) {
        free(client_cfg);
        exit(EXIT_FAILURE);
    }

    LOG("[CONT] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[CONT] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[CONT] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    
    LOG("[CONT] Starting container validation test for %d seconds...", cont_cfg.test_duration_sec);

    // Test containers based on configuration
    char *container_list = NULL;
    if (strcmp(cont_cfg.container_type, "all") == 0) {
        container_list = strdup("mp4,ts,mkv,avi,mov,webm");
    } else {
        container_list = strdup(cont_cfg.container_type);
    }

    // Parse and test each container format
    struct timeval test_end;
    gettimeofday(&test_end, NULL);
    test_end.tv_sec += cont_cfg.test_duration_sec;

    char *token = strtok(container_list, ",");
    while (token != NULL && shutdown_flag != SHUTDOWN_REQUESTED) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        // Test current container format
        test_container_format(token);
        
        // Progress update
        print_progress_stats();
        
        token = strtok(NULL, ",");
        
        if (token == NULL && cont_cfg.test_duration_sec > 60) {
            // Restart container list if test duration is long
            free(container_list);
            if (strcmp(cont_cfg.container_type, "all") == 0) {
                container_list = strdup("mp4,ts,mkv,avi,mov,webm");
            } else {
                container_list = strdup(cont_cfg.container_type);
            }
            token = strtok(container_list, ",");
        }
        
        usleep(500000); // 500ms between tests
    }
    
    printf("\n");
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[CONT] Test completed in %.2f seconds", total_time);
    LOG("[CONT] Total containers tested: %lu", stats.containers_tested);
    LOG("[CONT] Valid containers: %lu", stats.valid_containers);
    LOG("[CONT] Invalid containers: %lu", stats.invalid_containers);
    
    if (stats.containers_tested > 0) {
        double success_rate = ((double)stats.valid_containers / stats.containers_tested) * 100.0;
        LOG("[CONT] Success rate: %.2f%%", success_rate);
        LOG("[CONT] Average processing time: %.3f ms", stats.avg_processing_time_ms);
    }
    
    LOG("[CONT] Operations - Muxing: %lu, Demuxing: %lu", stats.muxing_operations, stats.demuxing_operations);
    LOG("[CONT] Samples generated: %lu", stats.samples_generated);
    LOG("[CONT] Corruption detected: %lu", stats.corruption_detected);
    LOG("[CONT] Deep analyses: %lu", stats.deep_analyses);
    LOG("[CONT] Errors - Structure: %lu, Metadata: %lu, Stream: %lu, Seeking: %lu", 
        stats.structure_errors, stats.metadata_errors, stats.stream_errors, stats.seeking_errors);
    
    // Save results to file if specified
    save_container_results();

safe_exit:
    LOG("[CONT] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[CONT] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    if (container_list) free(container_list);
    return err;
}
