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
#include <math.h>
#include <float.h>

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

// Codec test configuration
typedef struct {
    char codec_type[32];         // "h264", "h265", "av1", "vp9", "jpeg", "aac", "opus", "all"
    char profile[32];            // Codec profile
    char level[16];              // Codec level
    char preset[32];             // Encoding preset
    int width;                   // Video width
    int height;                  // Video height
    int fps;                     // Video FPS
    char pixel_format[16];       // Pixel format
    int bitrate_kbps;           // Target bitrate
    int gop_size;               // GOP size
    int b_frames;               // B-frame count
    int sample_rate;            // Audio sample rate
    int channels;               // Audio channels
    int bit_depth;              // Audio bit depth
    int test_duration_sec;
    int test_quality_levels;    // Test multiple quality levels
    int test_resolutions;       // Test multiple resolutions
    int test_framerates;        // Test multiple frame rates
    int test_encoding_modes;    // Test different encoding modes
    int validate_output;        // Validate encoded output
    char output_file[256];
    int verbose;
} codec_config_t;

static codec_config_t codec_cfg = {
    .codec_type = "h264",
    .profile = "baseline",
    .level = "3.1",
    .preset = "medium",
    .width = 1920,
    .height = 1080,
    .fps = 30,
    .pixel_format = "yuv420p",
    .bitrate_kbps = 5000,
    .gop_size = 30,
    .b_frames = 2,
    .sample_rate = 48000,
    .channels = 2,
    .bit_depth = 16,
    .test_duration_sec = 300,
    .test_quality_levels = 1,
    .test_resolutions = 1,
    .test_framerates = 1,
    .test_encoding_modes = 1,
    .validate_output = 1,
    .output_file = "",
    .verbose = 0
};

// Codec testing statistics
typedef struct {
    uint64_t frames_encoded;
    uint64_t frames_decoded;
    uint64_t encoding_errors;
    uint64_t decoding_errors;
    uint64_t validation_errors;
    uint64_t quality_tests;
    uint64_t resolution_tests;
    uint64_t framerate_tests;
    uint64_t mode_tests;
    uint64_t bytes_processed;
    double total_encoding_time_ms;
    double total_decoding_time_ms;
    double min_encoding_time_ms;
    double max_encoding_time_ms;
    double avg_encoding_time_ms;
    double min_decoding_time_ms;
    double max_decoding_time_ms;
    double avg_decoding_time_ms;
    double avg_bitrate_kbps;
    double avg_psnr;
    double avg_ssim;
    struct timeval start_time;
    char current_codec[64];
    char current_config[128];
} codec_stats_t;

static codec_stats_t stats = {0};

// Video quality test parameters
static int resolution_test_data[][2] = {
    {640, 480},    // SD
    {1280, 720},   // HD
    {1920, 1080},  // FHD
    {2560, 1440},  // QHD
    {3840, 2160}   // UHD
};

static int framerate_test_data[] = {24, 25, 30, 50, 60};
static int test_bitrates[] = {1000, 2500, 5000, 10000, 20000}; // kbps

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Codec-Specific Testing Application\n\n");
    printf("Codec Configuration:\n");
    printf("  -c, --codec <type>         Codec type: h264, h265, av1, vp9, jpeg, aac, opus, all (default: %s)\n", codec_cfg.codec_type);
    printf("  --profile <profile>        Codec profile (default: %s)\n", codec_cfg.profile);
    printf("  --level <level>            Codec level (default: %s)\n", codec_cfg.level);
    printf("  --preset <preset>          Encoding preset (default: %s)\n", codec_cfg.preset);
    printf("  -t, --test-duration <sec>  Test duration in seconds (default: %d)\n", codec_cfg.test_duration_sec);
    printf("\n  Video Parameters:\n");
    printf("  --width <pixels>           Video width (default: %d)\n", codec_cfg.width);
    printf("  --height <pixels>          Video height (default: %d)\n", codec_cfg.height);
    printf("  --fps <fps>                Frame rate (default: %d)\n", codec_cfg.fps);
    printf("  --pixel-format <format>    Pixel format (default: %s)\n", codec_cfg.pixel_format);
    printf("  --bitrate <kbps>           Target bitrate in kbps (default: %d)\n", codec_cfg.bitrate_kbps);
    printf("  --gop-size <frames>        GOP size (default: %d)\n", codec_cfg.gop_size);
    printf("  --b-frames <count>         B-frame count (default: %d)\n", codec_cfg.b_frames);
    printf("\n  Audio Parameters:\n");
    printf("  --sample-rate <hz>         Sample rate (default: %d)\n", codec_cfg.sample_rate);
    printf("  --channels <count>         Channel count (default: %d)\n", codec_cfg.channels);
    printf("  --bit-depth <bits>         Bit depth (default: %d)\n", codec_cfg.bit_depth);
    printf("\n  Testing Options:\n");
    printf("  --test-quality-levels      Test multiple quality levels (default: %s)\n", codec_cfg.test_quality_levels ? "enabled" : "disabled");
    printf("  --test-resolutions         Test multiple resolutions (default: %s)\n", codec_cfg.test_resolutions ? "enabled" : "disabled");
    printf("  --test-framerates          Test multiple frame rates (default: %s)\n", codec_cfg.test_framerates ? "enabled" : "disabled");
    printf("  --test-encoding-modes      Test different encoding modes (default: %s)\n", codec_cfg.test_encoding_modes ? "enabled" : "disabled");
    printf("  --validate-output          Validate encoded output (default: %s)\n", codec_cfg.validate_output ? "enabled" : "disabled");
    printf("  --no-quality               Disable quality level testing\n");
    printf("  --no-resolutions           Disable resolution testing\n");
    printf("  --no-framerates            Disable framerate testing\n");
    printf("  --no-modes                 Disable encoding mode testing\n");
    printf("  --no-validation            Disable output validation\n");
    printf("\n  Output:\n");
    printf("  -o, --output <file>        Save test results to file\n");
    printf("  -v, --verbose              Enable verbose output\n");
    printf("  -h, --help                 Show this help\n");
    printf("\n  Examples:\n");
    printf("    # Test H.264 with multiple quality levels\n");
    printf("    %s --codec h264 --test-quality-levels\n", prog_name);
    printf("\n    # Test H.265 with all resolutions and framerates\n");
    printf("    %s --codec h265 --test-resolutions --test-framerates\n", prog_name);
    printf("\n    # Test AV1 with specific profile and preset\n");
    printf("    %s --codec av1 --profile main --preset slow\n", prog_name);
}

int parse_arguments(int argc, char **argv) {
    static struct option long_options[] = {
        {"codec", required_argument, 0, 'c'},
        {"profile", required_argument, 0, 1001},
        {"level", required_argument, 0, 1002},
        {"preset", required_argument, 0, 1003},
        {"test-duration", required_argument, 0, 't'},
        {"width", required_argument, 0, 1004},
        {"height", required_argument, 0, 1005},
        {"fps", required_argument, 0, 1006},
        {"pixel-format", required_argument, 0, 1007},
        {"bitrate", required_argument, 0, 1008},
        {"gop-size", required_argument, 0, 1009},
        {"b-frames", required_argument, 0, 1010},
        {"sample-rate", required_argument, 0, 1011},
        {"channels", required_argument, 0, 1012},
        {"bit-depth", required_argument, 0, 1013},
        {"test-quality-levels", no_argument, 0, 1014},
        {"test-resolutions", no_argument, 0, 1015},
        {"test-framerates", no_argument, 0, 1016},
        {"test-encoding-modes", no_argument, 0, 1017},
        {"validate-output", no_argument, 0, 1018},
        {"no-quality", no_argument, 0, 1019},
        {"no-resolutions", no_argument, 0, 1020},
        {"no-framerates", no_argument, 0, 1021},
        {"no-modes", no_argument, 0, 1022},
        {"no-validation", no_argument, 0, 1023},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "c:t:o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'c':
                strncpy(codec_cfg.codec_type, optarg, sizeof(codec_cfg.codec_type) - 1);
                break;
            case 1001:
                strncpy(codec_cfg.profile, optarg, sizeof(codec_cfg.profile) - 1);
                break;
            case 1002:
                strncpy(codec_cfg.level, optarg, sizeof(codec_cfg.level) - 1);
                break;
            case 1003:
                strncpy(codec_cfg.preset, optarg, sizeof(codec_cfg.preset) - 1);
                break;
            case 't':
                codec_cfg.test_duration_sec = atoi(optarg);
                break;
            case 1004:
                codec_cfg.width = atoi(optarg);
                break;
            case 1005:
                codec_cfg.height = atoi(optarg);
                break;
            case 1006:
                codec_cfg.fps = atoi(optarg);
                break;
            case 1007:
                strncpy(codec_cfg.pixel_format, optarg, sizeof(codec_cfg.pixel_format) - 1);
                break;
            case 1008:
                codec_cfg.bitrate_kbps = atoi(optarg);
                break;
            case 1009:
                codec_cfg.gop_size = atoi(optarg);
                break;
            case 1010:
                codec_cfg.b_frames = atoi(optarg);
                break;
            case 1011:
                codec_cfg.sample_rate = atoi(optarg);
                break;
            case 1012:
                codec_cfg.channels = atoi(optarg);
                break;
            case 1013:
                codec_cfg.bit_depth = atoi(optarg);
                break;
            case 1014:
                codec_cfg.test_quality_levels = 1;
                break;
            case 1015:
                codec_cfg.test_resolutions = 1;
                break;
            case 1016:
                codec_cfg.test_framerates = 1;
                break;
            case 1017:
                codec_cfg.test_encoding_modes = 1;
                break;
            case 1018:
                codec_cfg.validate_output = 1;
                break;
            case 1019:
                codec_cfg.test_quality_levels = 0;
                break;
            case 1020:
                codec_cfg.test_resolutions = 0;
                break;
            case 1021:
                codec_cfg.test_framerates = 0;
                break;
            case 1022:
                codec_cfg.test_encoding_modes = 0;
                break;
            case 1023:
                codec_cfg.validate_output = 0;
                break;
            case 'o':
                strncpy(codec_cfg.output_file, optarg, sizeof(codec_cfg.output_file) - 1);
                break;
            case 'v':
                codec_cfg.verbose = 1;
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

// Generate test frame data
int generate_test_frame(uint8_t *buffer, int width, int height, int frame_num) {
    int frame_size = width * height * 3 / 2; // YUV420 format
    
    // Generate Y plane
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            // Moving pattern based on frame number
            int pattern = ((x + frame_num * 2) % 256) ^ ((y + frame_num) % 256);
            buffer[index] = pattern & 0xFF;
        }
    }
    
    // Generate U and V planes
    int uv_offset = width * height;
    for (int y = 0; y < height / 2; y++) {
        for (int x = 0; x < width / 2; x++) {
            int index = uv_offset + y * (width / 2) + x;
            buffer[index] = ((x + frame_num) % 256) & 0xFF; // U
            buffer[index + (width * height / 4)] = ((y + frame_num) % 256) & 0xFF; // V
        }
    }
    
    return frame_size;
}

// Generate test audio data
int generate_test_audio(uint8_t *buffer, int samples, int channels, int sample_rate, int frame_num) {
    int16_t *audio_data = (int16_t *)buffer;
    double frequency = 440.0; // A note
    
    for (int i = 0; i < samples; i++) {
        double time = (double)(frame_num * samples + i) / sample_rate;
        int16_t sample = (int16_t)(32767.0 * sin(2.0 * M_PI * frequency * time));
        
        for (int ch = 0; ch < channels; ch++) {
            audio_data[i * channels + ch] = sample;
        }
    }
    
    return samples * channels * sizeof(int16_t);
}

// Calculate PSNR between two frames
double calculate_psnr(const uint8_t *original, const uint8_t *compressed, int size) {
    uint64_t mse = 0;
    
    for (int i = 0; i < size; i++) {
        int diff = original[i] - compressed[i];
        mse += diff * diff;
    }
    
    if (mse == 0) return 100.0; // Perfect match
    
    double mean_mse = (double)mse / size;
    return 20.0 * log10(255.0 / sqrt(mean_mse));
}

// Simulate codec encoding
int simulate_codec_encoding(const char *codec, const uint8_t *input, int input_size, 
                           uint8_t *output, int max_output_size, int *output_size) {
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Simulate encoding process
    if (strcmp(codec, "h264") == 0) {
        // H.264 encoding simulation
        *output_size = input_size / 10; // ~10:1 compression
        if (*output_size > max_output_size) *output_size = max_output_size;
        
        // Simulate encoded data
        for (int i = 0; i < *output_size; i++) {
            output[i] = input[i % input_size] ^ (i & 0xFF);
        }
        
        usleep(5000); // 5ms encoding time
    } else if (strcmp(codec, "h265") == 0) {
        // H.265 encoding simulation
        *output_size = input_size / 15; // ~15:1 compression
        if (*output_size > max_output_size) *output_size = max_output_size;
        
        for (int i = 0; i < *output_size; i++) {
            output[i] = input[i % input_size] ^ ((i * 2) & 0xFF);
        }
        
        usleep(8000); // 8ms encoding time
    } else if (strcmp(codec, "av1") == 0) {
        // AV1 encoding simulation
        *output_size = input_size / 20; // ~20:1 compression
        if (*output_size > max_output_size) *output_size = max_output_size;
        
        for (int i = 0; i < *output_size; i++) {
            output[i] = input[i % input_size] ^ ((i * 3) & 0xFF);
        }
        
        usleep(15000); // 15ms encoding time
    } else {
        return -1; // Unsupported codec
    }
    
    gettimeofday(&end, NULL);
    double encoding_time = ((end.tv_sec - start.tv_sec) * 1000.0) +
                          ((end.tv_usec - start.tv_usec) / 1000.0);
    
    // Update statistics
    stats.frames_encoded++;
    stats.total_encoding_time_ms += encoding_time;
    
    if (stats.frames_encoded == 1) {
        stats.min_encoding_time_ms = stats.max_encoding_time_ms = encoding_time;
    } else {
        if (encoding_time < stats.min_encoding_time_ms) stats.min_encoding_time_ms = encoding_time;
        if (encoding_time > stats.max_encoding_time_ms) stats.max_encoding_time_ms = encoding_time;
    }
    
    stats.avg_encoding_time_ms = stats.total_encoding_time_ms / stats.frames_encoded;
    
    return 0;
}

// Simulate codec decoding
int simulate_codec_decoding(const char *codec, const uint8_t *input, int input_size,
                           uint8_t *output, int max_output_size, int *output_size) {
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Simulate decoding process
    if (strcmp(codec, "h264") == 0) {
        *output_size = input_size * 10; // Reverse compression
        if (*output_size > max_output_size) *output_size = max_output_size;
        
        for (int i = 0; i < *output_size; i++) {
            output[i] = input[i % input_size] ^ (i & 0xFF);
        }
        
        usleep(3000); // 3ms decoding time
    } else if (strcmp(codec, "h265") == 0) {
        *output_size = input_size * 15;
        if (*output_size > max_output_size) *output_size = max_output_size;
        
        for (int i = 0; i < *output_size; i++) {
            output[i] = input[i % input_size] ^ ((i * 2) & 0xFF);
        }
        
        usleep(4000); // 4ms decoding time
    } else if (strcmp(codec, "av1") == 0) {
        *output_size = input_size * 20;
        if (*output_size > max_output_size) *output_size = max_output_size;
        
        for (int i = 0; i < *output_size; i++) {
            output[i] = input[i % input_size] ^ ((i * 3) & 0xFF);
        }
        
        usleep(6000); // 6ms decoding time
    } else {
        return -1;
    }
    
    gettimeofday(&end, NULL);
    double decoding_time = ((end.tv_sec - start.tv_sec) * 1000.0) +
                          ((end.tv_usec - start.tv_usec) / 1000.0);
    
    // Update statistics
    stats.frames_decoded++;
    stats.total_decoding_time_ms += decoding_time;
    
    if (stats.frames_decoded == 1) {
        stats.min_decoding_time_ms = stats.max_decoding_time_ms = decoding_time;
    } else {
        if (decoding_time < stats.min_decoding_time_ms) stats.min_decoding_time_ms = decoding_time;
        if (decoding_time > stats.max_decoding_time_ms) stats.max_decoding_time_ms = decoding_time;
    }
    
    stats.avg_decoding_time_ms = stats.total_decoding_time_ms / stats.frames_decoded;
    
    return 0;
}

// Test codec with specific configuration
int test_codec_config(const char *codec, int width, int height, int fps, int bitrate) {
    snprintf(stats.current_config, sizeof(stats.current_config), 
             "%dx%d@%dfps %dkbps", width, height, fps, bitrate);
    
    if (codec_cfg.verbose) {
        LOG("[CODEC] Testing %s: %s", codec, stats.current_config);
    }
    
    int frame_size = width * height * 3 / 2; // YUV420
    uint8_t *input_frame = malloc(frame_size);
    uint8_t *encoded_data = malloc(frame_size);
    uint8_t *decoded_frame = malloc(frame_size);
    
    if (!input_frame || !encoded_data || !decoded_frame) {
        LOG("[CODEC] Memory allocation failed");
        free(input_frame);
        free(encoded_data);
        free(decoded_frame);
        return -1;
    }
    
    int errors = 0;
    int test_frames = fps * 5; // 5 seconds of video
    
    for (int frame = 0; frame < test_frames; frame++) {
        if (shutdown_flag == SHUTDOWN_REQUESTED) break;
        
        // Generate test frame
        generate_test_frame(input_frame, width, height, frame);
        
        // Encode frame
        int encoded_size;
        if (simulate_codec_encoding(codec, input_frame, frame_size, 
                                   encoded_data, frame_size, &encoded_size) != 0) {
            stats.encoding_errors++;
            errors++;
            continue;
        }
        
        stats.bytes_processed += encoded_size;
        
        // Decode frame if validation enabled
        if (codec_cfg.validate_output) {
            int decoded_size;
            if (simulate_codec_decoding(codec, encoded_data, encoded_size,
                                       decoded_frame, frame_size, &decoded_size) != 0) {
                stats.decoding_errors++;
                errors++;
                continue;
            }
            
            // Calculate quality metrics
            double psnr = calculate_psnr(input_frame, decoded_frame, frame_size);
            stats.avg_psnr = ((stats.avg_psnr * (stats.frames_decoded - 1)) + psnr) / stats.frames_decoded;
            
            if (psnr < 30.0) { // Low quality threshold
                stats.validation_errors++;
                if (codec_cfg.verbose) {
                    LOG("[CODEC] Low PSNR detected: %.2f dB", psnr);
                }
            }
        }
        
        // Update bitrate calculation
        double current_bitrate = (encoded_size * 8.0 * fps) / 1000.0; // kbps
        stats.avg_bitrate_kbps = ((stats.avg_bitrate_kbps * (stats.frames_encoded - 1)) + current_bitrate) / stats.frames_encoded;
    }
    
    free(input_frame);
    free(encoded_data);
    free(decoded_frame);
    
    if (codec_cfg.verbose) {
        LOG("[CODEC] %s %s: %d errors", codec, stats.current_config, errors);
    }
    
    return errors;
}

// Test codec with multiple quality levels
int test_quality_levels(const char *codec) {
    LOG("[CODEC] Testing quality levels for %s", codec);
    
    int errors = 0;
    for (int i = 0; i < 5; i++) {
        errors += test_codec_config(codec, codec_cfg.width, codec_cfg.height, 
                                   codec_cfg.fps, test_bitrates[i]);
        stats.quality_tests++;
    }
    
    return errors;
}

// Test codec with multiple resolutions
int test_resolutions(const char *codec) {
    LOG("[CODEC] Testing resolutions for %s", codec);
    
    int errors = 0;
    for (int i = 0; i < 5; i++) {
        errors += test_codec_config(codec, resolution_test_data[i][0], resolution_test_data[i][1],
                                   codec_cfg.fps, codec_cfg.bitrate_kbps);
        stats.resolution_tests++;
    }
    
    return errors;
}

// Test codec with multiple frame rates
int test_framerates(const char *codec) {
    LOG("[CODEC] Testing frame rates for %s", codec);
    
    int errors = 0;
    for (int i = 0; i < 5; i++) {
        errors += test_codec_config(codec, codec_cfg.width, codec_cfg.height,
                                   framerate_test_data[i], codec_cfg.bitrate_kbps);
        stats.framerate_tests++;
    }
    
    return errors;
}

// Test specific codec
int test_codec(const char *codec) {
    LOG("[CODEC] Testing codec: %s", codec);
    strncpy(stats.current_codec, codec, sizeof(stats.current_codec) - 1);
    
    int total_errors = 0;
    
    // Basic test
    total_errors += test_codec_config(codec, codec_cfg.width, codec_cfg.height,
                                     codec_cfg.fps, codec_cfg.bitrate_kbps);
    
    // Additional tests based on configuration
    if (codec_cfg.test_quality_levels) {
        total_errors += test_quality_levels(codec);
    }
    
    if (codec_cfg.test_resolutions) {
        total_errors += test_resolutions(codec);
    }
    
    if (codec_cfg.test_framerates) {
        total_errors += test_framerates(codec);
    }
    
    if (codec_cfg.test_encoding_modes) {
        // Test different encoding modes
        stats.mode_tests++;
        if (codec_cfg.verbose) {
            LOG("[CODEC] Testing encoding modes for %s", codec);
        }
    }
    
    return total_errors;
}

char* generate_codec_config() {
    char *config = malloc(1024);
    if (!config) {
        LOG("[CODEC] Failed to allocate memory for config");
        return NULL;
    }

    snprintf(config, 1024,
        "{\n"
        "  \"connection\": {\n"
        "    \"codec\": {\n"
        "      \"type\": \"%s\",\n"
        "      \"profile\": \"%s\",\n"
        "      \"level\": \"%s\",\n"
        "      \"preset\": \"%s\"\n"
        "    },\n"
        "    \"validation\": {\n"
        "      \"enableOutput\": %s\n"
        "    }\n"
        "  },\n"
        "  \"payload\": {\n"
        "    \"video\": {\n"
        "      \"width\": %d,\n"
        "      \"height\": %d,\n"
        "      \"fps\": %d,\n"
        "      \"pixelFormat\": \"%s\",\n"
        "      \"bitrate\": %d,\n"
        "      \"gopSize\": %d,\n"
        "      \"bFrames\": %d\n"
        "    },\n"
        "    \"audio\": {\n"
        "      \"sampleRate\": %d,\n"
        "      \"channels\": %d,\n"
        "      \"bitDepth\": %d\n"
        "    }\n"
        "  },\n"
        "  \"testing\": {\n"
        "    \"qualityLevels\": %s,\n"
        "    \"resolutions\": %s,\n"
        "    \"framerates\": %s,\n"
        "    \"encodingModes\": %s\n"
        "  }\n"
        "}",
        codec_cfg.codec_type,
        codec_cfg.profile,
        codec_cfg.level,
        codec_cfg.preset,
        codec_cfg.validate_output ? "true" : "false",
        codec_cfg.width,
        codec_cfg.height,
        codec_cfg.fps,
        codec_cfg.pixel_format,
        codec_cfg.bitrate_kbps,
        codec_cfg.gop_size,
        codec_cfg.b_frames,
        codec_cfg.sample_rate,
        codec_cfg.channels,
        codec_cfg.bit_depth,
        codec_cfg.test_quality_levels ? "true" : "false",
        codec_cfg.test_resolutions ? "true" : "false",
        codec_cfg.test_framerates ? "true" : "false",
        codec_cfg.test_encoding_modes ? "true" : "false");

    return config;
}

void print_progress_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = ((now.tv_sec - stats.start_time.tv_sec) +
                     (now.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    double encoding_fps = 0.0;
    if (stats.avg_encoding_time_ms > 0) {
        encoding_fps = 1000.0 / stats.avg_encoding_time_ms;
    }
    
    printf("\r[CODEC] %.1fs | Codec: %s | Encoded: %lu | Decoded: %lu | Enc FPS: %.1f | Avg PSNR: %.1f dB", 
           elapsed, stats.current_codec, stats.frames_encoded, stats.frames_decoded, 
           encoding_fps, stats.avg_psnr);
    fflush(stdout);
}

void save_codec_results() {
    if (strlen(codec_cfg.output_file) == 0) return;
    
    FILE *f = fopen(codec_cfg.output_file, "w");
    if (!f) {
        LOG("[CODEC] Failed to open output file: %s", codec_cfg.output_file);
        return;
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    fprintf(f, "# Codec-Specific Test Results\n");
    fprintf(f, "Codec: %s\n", codec_cfg.codec_type);
    fprintf(f, "Profile: %s\n", codec_cfg.profile);
    fprintf(f, "Level: %s\n", codec_cfg.level);
    fprintf(f, "Preset: %s\n", codec_cfg.preset);
    fprintf(f, "Test Duration: %.2f seconds\n", total_time);
    
    fprintf(f, "\nVideo Configuration:\n");
    fprintf(f, "Resolution: %dx%d\n", codec_cfg.width, codec_cfg.height);
    fprintf(f, "Frame Rate: %d fps\n", codec_cfg.fps);
    fprintf(f, "Pixel Format: %s\n", codec_cfg.pixel_format);
    fprintf(f, "Target Bitrate: %d kbps\n", codec_cfg.bitrate_kbps);
    fprintf(f, "GOP Size: %d\n", codec_cfg.gop_size);
    fprintf(f, "B Frames: %d\n", codec_cfg.b_frames);
    
    fprintf(f, "\nTesting Configuration:\n");
    fprintf(f, "Quality Level Tests: %s\n", codec_cfg.test_quality_levels ? "enabled" : "disabled");
    fprintf(f, "Resolution Tests: %s\n", codec_cfg.test_resolutions ? "enabled" : "disabled");
    fprintf(f, "Framerate Tests: %s\n", codec_cfg.test_framerates ? "enabled" : "disabled");
    fprintf(f, "Encoding Mode Tests: %s\n", codec_cfg.test_encoding_modes ? "enabled" : "disabled");
    fprintf(f, "Output Validation: %s\n", codec_cfg.validate_output ? "enabled" : "disabled");
    
    fprintf(f, "\nEncoding Statistics:\n");
    fprintf(f, "Frames Encoded: %lu\n", stats.frames_encoded);
    fprintf(f, "Frames Decoded: %lu\n", stats.frames_decoded);
    fprintf(f, "Encoding Errors: %lu\n", stats.encoding_errors);
    fprintf(f, "Decoding Errors: %lu\n", stats.decoding_errors);
    fprintf(f, "Validation Errors: %lu\n", stats.validation_errors);
    fprintf(f, "Bytes Processed: %lu\n", stats.bytes_processed);
    
    fprintf(f, "\nPerformance Metrics:\n");
    if (stats.frames_encoded > 0) {
        fprintf(f, "Average Encoding Time: %.3f ms\n", stats.avg_encoding_time_ms);
        fprintf(f, "Min Encoding Time: %.3f ms\n", stats.min_encoding_time_ms);
        fprintf(f, "Max Encoding Time: %.3f ms\n", stats.max_encoding_time_ms);
        fprintf(f, "Encoding FPS: %.2f\n", 1000.0 / stats.avg_encoding_time_ms);
    }
    
    if (stats.frames_decoded > 0) {
        fprintf(f, "Average Decoding Time: %.3f ms\n", stats.avg_decoding_time_ms);
        fprintf(f, "Min Decoding Time: %.3f ms\n", stats.min_decoding_time_ms);
        fprintf(f, "Max Decoding Time: %.3f ms\n", stats.max_decoding_time_ms);
        fprintf(f, "Decoding FPS: %.2f\n", 1000.0 / stats.avg_decoding_time_ms);
    }
    
    fprintf(f, "\nQuality Metrics:\n");
    fprintf(f, "Average Bitrate: %.2f kbps\n", stats.avg_bitrate_kbps);
    if (codec_cfg.validate_output && stats.frames_decoded > 0) {
        fprintf(f, "Average PSNR: %.3f dB\n", stats.avg_psnr);
        fprintf(f, "Average SSIM: %.4f\n", stats.avg_ssim);
    }
    
    fprintf(f, "\nTest Summary:\n");
    fprintf(f, "Quality Tests: %lu\n", stats.quality_tests);
    fprintf(f, "Resolution Tests: %lu\n", stats.resolution_tests);
    fprintf(f, "Framerate Tests: %lu\n", stats.framerate_tests);
    fprintf(f, "Mode Tests: %lu\n", stats.mode_tests);
    
    fclose(f);
    LOG("[CODEC] Test results saved to: %s", codec_cfg.output_file);
}

int main(int argc, char **argv) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    parse_arguments(argc, argv);

    LOG("[CODEC] Starting Codec-Specific Test");
    LOG("[CODEC] Codec: %s, Profile: %s, Level: %s, Preset: %s", 
        codec_cfg.codec_type, codec_cfg.profile, codec_cfg.level, codec_cfg.preset);
    LOG("[CODEC] Resolution: %dx%d@%dfps, Bitrate: %d kbps", 
        codec_cfg.width, codec_cfg.height, codec_cfg.fps, codec_cfg.bitrate_kbps);
    LOG("[CODEC] Testing - Quality: %s, Resolutions: %s, Framerates: %s, Modes: %s", 
        codec_cfg.test_quality_levels ? "enabled" : "disabled",
        codec_cfg.test_resolutions ? "enabled" : "disabled",
        codec_cfg.test_framerates ? "enabled" : "disabled",
        codec_cfg.test_encoding_modes ? "enabled" : "disabled");

    // Initialize random seed
    srand(time(NULL));

    // Generate client configuration
    client_cfg = malloc(256);
    snprintf(client_cfg, 256,
        "{\n"
        "  \"apiVersion\": \"v1\",\n"
        "  \"apiConnectionString\": \"Server=127.0.0.1; Port=8002\"\n"
        "}");

    // Generate codec-specific connection configuration
    conn_cfg = generate_codec_config();
    if (!conn_cfg) {
        free(client_cfg);
        exit(EXIT_FAILURE);
    }

    LOG("[CODEC] Connection config:\n%s", conn_cfg);

    /* Initialize mcm client */
    int err = mesh_create_client(&client, client_cfg);
    if (err) {
        LOG("[CODEC] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[CODEC] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    // Initialize statistics
    gettimeofday(&stats.start_time, NULL);
    stats.min_encoding_time_ms = stats.min_decoding_time_ms = DBL_MAX;
    
    LOG("[CODEC] Starting codec test for %d seconds...", codec_cfg.test_duration_sec);

    // Test codecs based on configuration
    if (strcmp(codec_cfg.codec_type, "all") == 0) {
        char *codecs[] = {"h264", "h265", "av1", "vp9", "jpeg"};
        int num_codecs = 5;
        
        for (int i = 0; i < num_codecs && shutdown_flag != SHUTDOWN_REQUESTED; i++) {
            test_codec(codecs[i]);
            print_progress_stats();
        }
    } else {
        test_codec(codec_cfg.codec_type);
    }
    
    // Main test loop for continuous testing
    struct timeval test_end;
    gettimeofday(&test_end, NULL);
    test_end.tv_sec += codec_cfg.test_duration_sec;

    while (shutdown_flag != SHUTDOWN_REQUESTED) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        if (now.tv_sec > test_end.tv_sec || 
            (now.tv_sec == test_end.tv_sec && now.tv_usec >= test_end.tv_usec)) {
            break;
        }
        
        // Continue testing or perform additional validation
        print_progress_stats();
        usleep(1000000); // 1 second
    }
    
    printf("\n");
    
    // Final statistics
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = ((end_time.tv_sec - stats.start_time.tv_sec) +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0);
    
    LOG("[CODEC] Test completed in %.2f seconds", total_time);
    LOG("[CODEC] Total frames encoded: %lu", stats.frames_encoded);
    LOG("[CODEC] Total frames decoded: %lu", stats.frames_decoded);
    LOG("[CODEC] Encoding errors: %lu", stats.encoding_errors);
    LOG("[CODEC] Decoding errors: %lu", stats.decoding_errors);
    
    if (stats.frames_encoded > 0) {
        LOG("[CODEC] Average encoding time: %.3f ms (%.1f fps)", 
            stats.avg_encoding_time_ms, 1000.0 / stats.avg_encoding_time_ms);
    }
    
    if (stats.frames_decoded > 0) {
        LOG("[CODEC] Average decoding time: %.3f ms (%.1f fps)", 
            stats.avg_decoding_time_ms, 1000.0 / stats.avg_decoding_time_ms);
    }
    
    if (codec_cfg.validate_output && stats.frames_decoded > 0) {
        LOG("[CODEC] Average PSNR: %.3f dB", stats.avg_psnr);
    }
    
    LOG("[CODEC] Average bitrate: %.2f kbps (target: %d kbps)", 
        stats.avg_bitrate_kbps, codec_cfg.bitrate_kbps);
    
    // Save results to file if specified
    save_codec_results();

safe_exit:
    LOG("[CODEC] Shutting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[CODEC] Shutting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    return err;
}
