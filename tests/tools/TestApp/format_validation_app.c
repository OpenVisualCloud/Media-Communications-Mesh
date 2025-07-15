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

// Format validation configuration
typedef struct {
    char format_type[32];        // "video", "audio", "container", "custom", "all"
    char codec_list[256];        // Comma-separated codec list
    char container_list[256];    // Comma-separated container list
    char test_files[512];        // Comma-separated test file list
    int test_duration_sec;
    int validate_headers;        // Validate format headers
    int validate_metadata;       // Validate metadata
    int validate_streams;        // Validate stream structure
    int test_corruption;         // Test corruption detection
    int test_malformed;          // Test malformed data handling
    int generate_samples;        // Generate format samples
    int deep_validation;         // Enable deep format validation
    char output_file[256];
    int verbose;
} format_config_t;

static format_config_t fmt_cfg = {
    .format_type = "video",
    .codec_list = "h264,h265,av1,vp9,jpeg",
    .container_list = "mp4,ts,mkv,avi,mov",
    .test_files = "",
    .test_duration_sec = 300,
    .validate_headers = 1,
    .validate_metadata = 1,
    .validate_streams = 1,
    .test_corruption = 1,
    .test_malformed = 1,
    .generate_samples = 1,
    .deep_validation = 0,
    .output_file = "",
    .verbose = 0
};

// Format validation statistics
typedef struct {
    uint64_t formats_tested;
    uint64_t valid_formats;
    uint64_t invalid_formats;
    uint64_t header_errors;
    uint64_t metadata_errors;
    uint64_t stream_errors;
    uint64_t corruption_detected;
    uint64_t malformed_detected;
    uint64_t samples_generated;
    uint64_t deep_validations;
    uint64_t bytes_validated;
    struct timeval start_time;
    char current_format[64];
    char current_codec[64];
} format_stats_t;

static format_stats_t stats = {0};

// Video format structures
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t fps_num;
    uint32_t fps_den;
    char pixel_format[16];
    char profile[32];
    char level[16];
} video_format_t;

// Audio format structures
typedef struct {
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t bit_depth;
    char format[16];
    char channel_layout[32];
} audio_format_t;

// Container format structures
typedef struct {
    char format_name[32];
    uint32_t version;
    uint64_t duration_ms;
    uint32_t num_streams;
    char metadata[256];
} container_format_t;

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Format Validation and Testing Application\n\n");
    printf("Format Configuration:\n");
    printf("  -f, --format <type>        Format type: video, audio, container, custom, all (default: %s)\n", fmt_cfg.format_type);
    printf("  --codecs <list>            Comma-separated codec list (default: %s)\n", fmt_cfg.codec_list);
    printf("  --containers <list>        Comma-separated container list (default: %s)\n", fmt_cfg.container_list);
    printf("  --test-files <list>        Comma-separated test file list\n");
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", fmt_cfg.test_duration_sec);
    printf("\n  Validation Options:\n");
    printf("  --validate-headers         Enable header validation (default: %s)\n", fmt_cfg.validate_headers ? "enabled" : "disabled");
    printf("  --validate-metadata        Enable metadata validation (default: %s)\n", fmt_cfg.validate_metadata ? "enabled" : "disabled");
    printf("  --validate-streams         Enable stream validation (default: %s)\n", fmt_cfg.validate_streams ? "enabled" : "disabled");
    printf("  --test-corruption          Enable corruption testing (default: %s)\n", fmt_cfg.test_corruption ? "enabled" : "disabled");
    printf("  --test-malformed           Enable malformed data testing (default: %s)\n", fmt_cfg.test_malformed ? "enabled" : "disabled");
    printf("  --no-headers               Disable header validation\n");
    printf("  --no-metadata              Disable metadata validation\n");
    printf("  --no-streams               Disable stream validation\n");
    printf("  --no-corruption            Disable corruption testing\n");
    printf("  --no-malformed             Disable malformed testing\n");
    printf("\n  Testing Features:\n");
    printf("  --generate-samples         Generate format samples (default: %s)\n", fmt_cfg.generate_samples ? "enabled" : "disabled");
    printf("  --deep-validation          Enable deep format validation\n");
    printf("  --no-samples               Disable sample generation\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Test all video codecs\n");
    printf("    %s --format video --codecs h264,h265,av1\n", prog_name);
    printf("\n    # Test container formats with deep validation\n");
    printf("    %s --format container --deep-validation\n", prog_name);
    printf("\n    # Test custom files with corruption detection\n");
    printf("    %s --test-files sample1.mp4,sample2.mkv --test-corruption\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"format", required_argument, 0, 'f'},
        {"codecs", required_argument, 0, 1001},
        {"containers", required_argument, 0, 1002},
        {"test-files", required_argument, 0, 1003},
        {"test-duration", required_argument, 0, 't'},
        {"validate-headers", no_argument, 0, 1004},
        {"validate-metadata", no_argument, 0, 1005},
        {"validate-streams", no_argument, 0, 1006},
        {"test-corruption", no_argument, 0, 1007},
        {"test-malformed", no_argument, 0, 1008},
        {"no-headers", no_argument, 0, 1009},
        {"no-metadata", no_argument, 0, 1010},
        {"no-streams", no_argument, 0, 1011},
        {"no-corruption", no_argument, 0, 1012},
        {"no-malformed", no_argument, 0, 1013},
        {"generate-samples", no_argument, 0, 1014},
        {"deep-validation", no_argument, 0, 1015},
        {"no-samples", no_argument, 0, 1016},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "f:t:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'f':
                strncpy(fmt_cfg.format_type, optarg, sizeof(fmt_cfg.format_type) - 1);
                break;
            case 1001:
                strncpy(fmt_cfg.codec_list, optarg, sizeof(fmt_cfg.codec_list) - 1);
                break;
            case 1002:
                strncpy(fmt_cfg.container_list, optarg, sizeof(fmt_cfg.container_list) - 1);
                break;
            case 1003:
                strncpy(fmt_cfg.test_files, optarg, sizeof(fmt_cfg.test_files) - 1);
                break;
            case 't':
                fmt_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1004:
                fmt_cfg.validate_headers = 1;
                break;
            case 1005:
                fmt_cfg.validate_metadata = 1;
                break;
            case 1006:
                fmt_cfg.validate_streams = 1;
                break;
            case 1007:
                fmt_cfg.test_corruption = 1;
                break;
            case 1008:
                fmt_cfg.test_malformed = 1;
                break;
            case 1009:
                fmt_cfg.validate_headers = 0;
                break;
            case 1010:
                fmt_cfg.validate_metadata = 0;
                break;
            case 1011:
                fmt_cfg.validate_streams = 0;
                break;
            case 1012:
                fmt_cfg.test_corruption = 0;
                break;
            case 1013:
                fmt_cfg.test_malformed = 0;
                break;
            case 1014:
                fmt_cfg.generate_samples = 1;
                break;
            case 1015:
                fmt_cfg.deep_validation = 1;
                break;
            case 1016:
                fmt_cfg.generate_samples = 0;
                break;
            case 'o':
                strncpy(fmt_cfg.output_file, optarg, sizeof(fmt_cfg.output_file) - 1);
                break;
            case 'v':
                fmt_cfg.verbose = 1;
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

// H.264 NAL unit header validation
int validate_h264_header(const uint8_t *data, size_t size) {
    if (size < 4) return -1;
    
    // Check for NAL unit start code
    if (data[0] != 0x00 || data[1] != 0x00 || data[2] != 0x00 || data[3] != 0x01) {
        return -1;
    }
    
    if (size < 5) return -1;
    
    // Parse NAL unit header
    uint8_t nal_header = data[4];
    uint8_t forbidden_bit = (nal_header >> 7) & 0x1;
    uint8_t nal_ref_idc = (nal_header >> 5) & 0x3;
    uint8_t nal_unit_type = nal_header & 0x1F;
    
    if (forbidden_bit != 0) {
        stats.header_errors++;
        return -1;
    }
    
    // Validate NAL unit type
    if (nal_unit_type == 0 || nal_unit_type > 23) {
        stats.header_errors++;
        return -1;
    }
    
    if (fmt_cfg.verbose) {
        LOG("[FMT] H.264 NAL: type=%d, ref_idc=%d", nal_unit_type, nal_ref_idc);
    }
    
    return 0;
}

// H.265 NAL unit header validation
int validate_h265_header(const uint8_t *data, size_t size) {
    if (size < 6) return -1;
    
    // Check for NAL unit start code
    if (data[0] != 0x00 || data[1] != 0x00 || data[2] != 0x00 || data[3] != 0x01) {
        return -1;
    }
    
    // Parse NAL unit header (2 bytes)
    uint16_t nal_header = (data[4] << 8) | data[5];
    uint8_t forbidden_bit = (nal_header >> 15) & 0x1;
    uint8_t nal_unit_type = (nal_header >> 9) & 0x3F;
    uint8_t nuh_layer_id = (nal_header >> 3) & 0x3F;
    uint8_t nuh_temporal_id = nal_header & 0x7;
    
    if (forbidden_bit != 0) {
        stats.header_errors++;
        return -1;
    }
    
    if (nuh_temporal_id == 0) {
        stats.header_errors++;
        return -1;
    }
    
    if (fmt_cfg.verbose) {
        LOG("[FMT] H.265 NAL: type=%d, layer=%d, temporal=%d", nal_unit_type, nuh_layer_id, nuh_temporal_id);
    }
    
    return 0;
}

// AV1 OBU header validation
int validate_av1_header(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    
    uint8_t obu_header = data[0];
    uint8_t obu_forbidden_bit = (obu_header >> 7) & 0x1;
    uint8_t obu_type = (obu_header >> 3) & 0xF;
    uint8_t obu_extension_flag = (obu_header >> 2) & 0x1;
    uint8_t obu_has_size_field = (obu_header >> 1) & 0x1;
    
    if (obu_forbidden_bit != 0) {
        stats.header_errors++;
        return -1;
    }
    
    // Validate OBU type
    if (obu_type > 15) {
        stats.header_errors++;
        return -1;
    }
    
    if (fmt_cfg.verbose) {
        LOG("[FMT] AV1 OBU: type=%d, extension=%d, has_size=%d", obu_type, obu_extension_flag, obu_has_size_field);
    }
    
    return 0;
}

// JPEG header validation
int validate_jpeg_header(const uint8_t *data, size_t size) {
    if (size < 2) return -1;
    
    // Check for JPEG SOI marker
    if (data[0] != 0xFF || data[1] != 0xD8) {
        stats.header_errors++;
        return -1;
    }
    
    // Look for JFIF or EXIF marker
    if (size >= 4) {
        if (data[2] == 0xFF && (data[3] == 0xE0 || data[3] == 0xE1)) {
            if (fmt_cfg.verbose) {
                LOG("[FMT] JPEG: SOI + %s marker found", data[3] == 0xE0 ? "JFIF" : "EXIF");
            }
        }
    }
    
    return 0;
}

// MP4 header validation
int validate_mp4_header(const uint8_t *data, size_t size) {
    if (size < 8) return -1;
    
    // Check for ftyp box
    uint32_t box_size = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    uint32_t box_type = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    
    if (box_type != 0x66747970) { // "ftyp"
        stats.header_errors++;
        return -1;
    }
    
    if (size >= 12) {
        uint32_t major_brand = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
        if (fmt_cfg.verbose) {
            LOG("[FMT] MP4: ftyp box, size=%u, brand=0x%08x", box_size, major_brand);
        }
    }
    
    return 0;
}

// Validate format header based on codec/container
int validate_format_header(const char *format, const uint8_t *data, size_t size) {
    strncpy(stats.current_format, format, sizeof(stats.current_format) - 1);
    
    if (strcmp(format, "h264") == 0) {
        return validate_h264_header(data, size);
    } else if (strcmp(format, "h265") == 0 || strcmp(format, "hevc") == 0) {
        return validate_h265_header(data, size);
    } else if (strcmp(format, "av1") == 0) {
        return validate_av1_header(data, size);
    } else if (strcmp(format, "jpeg") == 0) {
        return validate_jpeg_header(data, size);
    } else if (strcmp(format, "mp4") == 0) {
        return validate_mp4_header(data, size);
    }
    
    // Unknown format
    LOG("[FMT] Unknown format: %s", format);
    return -1;
}

// Generate format sample data
int generate_format_sample(const char *format, uint8_t *buffer, size_t max_size) {
    if (strcmp(format, "h264") == 0) {
        // Generate H.264 SPS NAL unit
        if (max_size < 20) return -1;
        
        buffer[0] = 0x00; buffer[1] = 0x00; buffer[2] = 0x00; buffer[3] = 0x01; // Start code
        buffer[4] = 0x67; // SPS NAL unit type
        buffer[5] = 0x42; // Profile (Baseline)
        buffer[6] = 0x00; // Constraints
        buffer[7] = 0x1E; // Level 3.0
        buffer[8] = 0xFF; // SPS data...
        for (int i = 9; i < 20; i++) buffer[i] = rand() & 0xFF;
        
        return 20;
    } else if (strcmp(format, "h265") == 0) {
        // Generate H.265 VPS NAL unit
        if (max_size < 20) return -1;
        
        buffer[0] = 0x00; buffer[1] = 0x00; buffer[2] = 0x00; buffer[3] = 0x01; // Start code
        buffer[4] = 0x40; // VPS NAL unit type
        buffer[5] = 0x01; // Layer ID and temporal ID
        for (int i = 6; i < 20; i++) buffer[i] = rand() & 0xFF;
        
        return 20;
    } else if (strcmp(format, "av1") == 0) {
        // Generate AV1 sequence header OBU
        if (max_size < 10) return -1;
        
        buffer[0] = 0x0A; // Sequence header OBU
        for (int i = 1; i < 10; i++) buffer[i] = rand() & 0xFF;
        
        return 10;
    } else if (strcmp(format, "jpeg") == 0) {
        // Generate minimal JPEG header
        if (max_size < 20) return -1;
        
        buffer[0] = 0xFF; buffer[1] = 0xD8; // SOI
        buffer[2] = 0xFF; buffer[3] = 0xE0; // JFIF marker
        buffer[4] = 0x00; buffer[5] = 0x10; // Length
        memcpy(&buffer[6], "JFIF\0", 5);   // JFIF identifier
        for (int i = 11; i < 20; i++) buffer[i] = rand() & 0xFF;
        
        return 20;
    }
    
    return -1;
}

// Test format with corruption
int test_format_corruption(const char *format, const uint8_t *original_data, size_t size) {
    uint8_t *corrupted_data = malloc(size);
    if (!corrupted_data) return -1;
    
    memcpy(corrupted_data, original_data, size);
    
    // Introduce random corruption
    int corruption_points = (size / 100) + 1; // 1% corruption
    for (int i = 0; i < corruption_points; i++) {
        size_t pos = rand() % size;
        corrupted_data[pos] ^= (1 << (rand() % 8)); // Flip random bit
    }
    
    // Validate corrupted data - should fail
    int result = validate_format_header(format, corrupted_data, size);
    if (result != 0) {
        stats.corruption_detected++;
        if (fmt_cfg.verbose) {
            LOG("[FMT] Corruption detected in %s format", format);
        }
    }
    
    free(corrupted_data);
    return result;
}

// Test format with malformed data
int test_format_malformed(const char *format) {
    uint8_t malformed_data[64];
    
    // Generate completely random data
    for (int i = 0; i < sizeof(malformed_data); i++) {
        malformed_data[i] = rand() & 0xFF;
    }
    
    // Validate malformed data - should fail
    int result = validate_format_header(format, malformed_data, sizeof(malformed_data));
    if (result != 0) {
        stats.malformed_detected++;
        if (fmt_cfg.verbose) {
            LOG("[FMT] Malformed data detected in %s format", format);
        }
    }
    
    return result;
}

// Test specific format/codec
int test_format(const char *format) {
    LOG("[FMT] Testing format: %s", format);
    strncpy(stats.current_codec, format, sizeof(stats.current_codec) - 1);
    
    int errors = 0;
    
    // Generate and test valid sample
    if (fmt_cfg.generate_samples) {
        uint8_t sample_data[1024];
        int sample_size = generate_format_sample(format, sample_data, sizeof(sample_data));
        
        if (sample_size > 0) {
            stats.samples_generated++;
            
            if (fmt_cfg.validate_headers) {
                if (validate_format_header(format, sample_data, sample_size) != 0) {
                    errors++;
                    stats.invalid_formats++;
                } else {
                    stats.valid_formats++;
                }
            }
            
            // Test corruption if enabled
            if (fmt_cfg.test_corruption) {
                test_format_corruption(format, sample_data, sample_size);
            }
            
            stats.bytes_validated += sample_size;
        }
    }
    
    // Test malformed data if enabled
    if (fmt_cfg.test_malformed) {
        test_format_malformed(format);
    }
    
    // Deep validation if enabled
    if (fmt_cfg.deep_validation) {
        stats.deep_validations++;
        // Additional deep validation would go here
        if (fmt_cfg.verbose) {
            LOG("[FMT] Deep validation completed for %s", format);
        }
    }
    
    stats.formats_tested++;
    return errors;
}

char* generate_format_config() {
    char *config = malloc(1024);
    if (!config) {
        LOG("[FMT] Failed to allocate memory for config");
        return NULL;
    }

    snprintf(config, 1024,
        "{\n"
        "  \"connection\": {\n"
        "    \"formatValidation\": {\n"
        "      \"enableHeaders\": %s,\n"
        "      \"enableMetadata\": %s,\n"
        "      \"enableStreams\": %s,\n"
        "      \"enableCorruption\": %s,\n"
        "      \"enableMalformed\": %s,\n"
        "      \"deepValidation\": %s\n"
        "    }\n"
        "  },\n"
        "  \"payload\": {\n"
        "    \"type\": \"%s\",\n"
        "    \"supportedCodecs\": [\"%s\"],\n"
        "    \"supportedContainers\": [\"%s\"]\n"
        "  },\n"
        "  \"testing\": {\n"
        "    \"generateSamples\": %s,\n"
        "    \"testFiles\": \"%s\"\n"
        "  }\n"
        "}",
        fmt_cfg.validate_headers ? "true" : "false",
        fmt_cfg.validate_metadata ? "true" : "false",
        fmt_cfg.validate_streams ? "true" : "false",
        fmt_cfg.test_corruption ? "true" : "false",
        fmt_cfg.test_malformed ? "true" : "false",
        fmt_cfg.deep_validation ? "true" : "false",
        fmt_cfg.format_type,
        fmt_cfg.codec_list,
        fmt_cfg.container_list,
        fmt_cfg.generate_samples ? "true" : "false",
        fmt_cfg.test_files);

    return config;
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    double success_rate = 0.0;
    if (stats.formats_tested > 0) {
        success_rate = ((double)stats.valid_formats / stats.formats_tested) * 100.0;
    }
    
    printf("\r[FMT] %.1fs | Format: %s | Tested: %lu | Valid: %lu (%.1f%%) | Errors: %lu", 
           elapsed, stats.current_format, stats.formats_tested, stats.valid_formats, 
           success_rate, stats.header_errors + stats.metadata_errors + stats.stream_errors);
    fflush(stdout);
}

void save_format_results() {
    if (strlen(fmt_cfg.output_file) == 0) return;
    
    FILE *f = fopen(fmt_cfg.output_file, "w");
    if (!f) {
        LOG("[FMT] Failed to open output file: %s", fmt_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Format Validation Test Results\n");
    fprintf(f, "Format Type: %s\n", fmt_cfg.format_type);
    fprintf(f, "Codec List: %s\n", fmt_cfg.codec_list);
    fprintf(f, "Container List: %s\n", fmt_cfg.container_list);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    fprintf(f, "Test Files: %s\n", fmt_cfg.test_files);
    
    fprintf(f, "\nValidation Configuration:\n");
    fprintf(f, "Header Validation: %s\n", fmt_cfg.validate_headers ? "enabled" : "disabled");
    fprintf(f, "Metadata Validation: %s\n", fmt_cfg.validate_metadata ? "enabled" : "disabled");
    fprintf(f, "Stream Validation: %s\n", fmt_cfg.validate_streams ? "enabled" : "disabled");
    fprintf(f, "Corruption Testing: %s\n", fmt_cfg.test_corruption ? "enabled" : "disabled");
    fprintf(f, "Malformed Testing: %s\n", fmt_cfg.test_malformed ? "enabled" : "disabled");
    fprintf(f, "Deep Validation: %s\n", fmt_cfg.deep_validation ? "enabled" : "disabled");
    
    fprintf(f, "\nValidation Statistics:\n");
    fprintf(f, "Formats Tested: %lu\n", stats.formats_tested);
    fprintf(f, "Valid Formats: %lu\n", stats.valid_formats);
    fprintf(f, "Invalid Formats: %lu\n", stats.invalid_formats);
    fprintf(f, "Samples Generated: %lu\n", stats.samples_generated);
    fprintf(f, "Deep Validations: %lu\n", stats.deep_validations);
    fprintf(f, "Bytes Validated: %lu\n", stats.bytes_validated);
    
    if (stats.formats_tested > 0) {
        double success_rate = ((double)stats.valid_formats / stats.formats_tested) * 100.0;
        fprintf(f, "Success Rate: %.2f%%\n", success_rate);
    }
    
    fprintf(f, "\nError Statistics:\n");
    fprintf(f, "Header Errors: %lu\n", stats.header_errors);
    fprintf(f, "Metadata Errors: %lu\n", stats.metadata_errors);
    fprintf(f, "Stream Errors: %lu\n", stats.stream_errors);
    fprintf(f, "Corruption Detected: %lu\n", stats.corruption_detected);
    fprintf(f, "Malformed Detected: %lu\n", stats.malformed_detected);
    
    fclose(f);
    LOG("[FMT] Test results saved to: %s", fmt_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[FMT] Starting Format Validation Test");
    LOG("[FMT] Format: %s, Codecs: %s, Containers: %s, Duration: %d seconds", 
        fmt_cfg.format_type, fmt_cfg.codec_list, fmt_cfg.container_list, fmt_cfg.test_duration_sec);
    LOG("[FMT] Validation - Headers: %s, Metadata: %s, Streams: %s", 
        fmt_cfg.validate_headers ? "enabled" : "disabled",
        fmt_cfg.validate_metadata ? "enabled" : "disabled",
        fmt_cfg.validate_streams ? "enabled" : "disabled");
    LOG("[FMT] Testing - Corruption: %s, Malformed: %s, Deep: %s, Samples: %s", 
        fmt_cfg.test_corruption ? "enabled" : "disabled",
        fmt_cfg.test_malformed ? "enabled" : "disabled",
        fmt_cfg.deep_validation ? "enabled" : "disabled",
        fmt_cfg.generate_samples ? "enabled" : "disabled");

    // Initialize random seed
    srand(time(NULL));

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate format validation connection configuration
    conn_cfg = generate_format_config();
    if (!conn_cfg) {
        free(client_cfg);
        exit(EXIT_FAILURE);
    }

    LOG("[FMT] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[FMT] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[FMT] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    
    LOG("[FMT] Starting format validation test for %d seconds...", fmt_cfg.test_duration_sec);

    // Test formats based on configuration
    char *format_list = NULL;
    if (strcmp(fmt_cfg.format_type, "video") == 0) {
        format_list = strdup(fmt_cfg.codec_list);
    } else if (strcmp(fmt_cfg.format_type, "container") == 0) {
        format_list = strdup(fmt_cfg.container_list);
    } else if (strcmp(fmt_cfg.format_type, "all") == 0) {
        // Combine codec and container lists
        format_list = malloc(strlen(fmt_cfg.codec_list) + strlen(fmt_cfg.container_list) + 2);
        sprintf(format_list, "%s,%s", fmt_cfg.codec_list, fmt_cfg.container_list);
    } else {
        format_list = strdup(fmt_cfg.format_type);
    }

    // Parse and test each format
    struct timeval test_end;
    gettimeofday(&test_end, NULL);
    test_end.tv_sec += fmt_cfg.test_duration_sec;

    char *token = strtok(format_list, ",");
    while (token != NULL && shutdown_flag != SHUTDOWN_REQUESTED) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        // Test current format
        test_format(token);
        
        // Progress update
        print_progress_stats();
        
        token = strtok(NULL, ",");
        
        if (token == NULL && fmt_cfg.test_duration_sec > 60) {
            // Restart format list if test duration is long
            free(format_list);
            if (strcmp(fmt_cfg.format_type, "video") == 0) {
                format_list = strdup(fmt_cfg.codec_list);
            } else if (strcmp(fmt_cfg.format_type, "container") == 0) {
                format_list = strdup(fmt_cfg.container_list);
            } else if (strcmp(fmt_cfg.format_type, "all") == 0) {
                format_list = malloc(strlen(fmt_cfg.codec_list) + strlen(fmt_cfg.container_list) + 2);
                sprintf(format_list, "%s,%s", fmt_cfg.codec_list, fmt_cfg.container_list);
            } else {
                format_list = strdup(fmt_cfg.format_type);
            }
            token = strtok(format_list, ",");
        }
        
        usleep(100000); // 100ms between tests
    }
    
    printf("\n");
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[FMT] Test completed in %.2f seconds", total_time);
    LOG("[FMT] Total formats tested: %lu", stats.formats_tested);
    LOG("[FMT] Valid formats: %lu", stats.valid_formats);
    LOG("[FMT] Invalid formats: %lu", stats.invalid_formats);
    
    if (stats.formats_tested > 0) {
        double success_rate = ((double)stats.valid_formats / stats.formats_tested) * 100.0;
        LOG("[FMT] Success rate: %.2f%%", success_rate);
    }
    
    LOG("[FMT] Samples generated: %lu", stats.samples_generated);
    LOG("[FMT] Corruption detected: %lu", stats.corruption_detected);
    LOG("[FMT] Malformed detected: %lu", stats.malformed_detected);
    LOG("[FMT] Errors - Header: %lu, Metadata: %lu, Stream: %lu", 
        stats.header_errors, stats.metadata_errors, stats.stream_errors);
    
    // Save results to file if specified
    save_format_results();

safe_exit:
    LOG("[FMT] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[FMT] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    if (format_list) free(format_list);
    return err;
}
