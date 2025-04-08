/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <jansson.h>
#include "input.h"
#include "mesh_dp.h"

#define S_TO_US_RATIO (int)1000000
#define MS_TO_US_RATIO (int)1000
int input_loop = 0;

long int parse_time_string_to_us(const char *input);

char *input_parse_file_to_string(const char *file_name) {
    FILE *input_fp = fopen(file_name, "rb");
    if (input_fp == NULL) {
        perror("Failed to open a file");
        exit(EXIT_FAILURE);
    }

    // Seek to the end of the file to determine its size
    fseek(input_fp, 0, SEEK_END);
    long file_size = ftell(input_fp);
    fseek(input_fp, 0, SEEK_SET); // Rewind to the beginning of the file

    // Allocate memory to hold the file contents plus a null terminator
    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        perror("Failed to allocate memory");
        fclose(input_fp);
        return NULL;
    }

    // Read the file into the buffer
    size_t read_size = fread(buffer, 1, file_size, input_fp);
    if (read_size != file_size) {
        perror("Failed to read file");
        free(buffer);
        fclose(input_fp);
        return NULL;
    }

    // Null-terminate the buffer
    buffer[file_size] = '\0';

    // Close the file
    fclose(input_fp);

    // Return the buffer as a const char*
    return buffer;
}

#define MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE  0 ///< planar YUV 4:2:2, 10bit, "yuv422p10le"
#define MESH_VIDEO_PIXEL_FORMAT_V210              1 ///< packed YUV 4:2:2, 10bit, "v210"
#define MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10 2 ///< packed RFC4175 compliant YUV 4:2:2, 10bit, "yuv422p10rfc4175"

int get_video_params(const char *json_string, video_params *params) {

    json_t *root;
    json_error_t error;
    json_t *video_value;

    root = json_loads(json_string, 0, &error);
    if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        json_decref(root);
        return 1;
    }

    json_t *payload = json_object_get(root, "payload");
    if (!payload) {
        fprintf(stderr, "error: key not found\n");
        json_decref(payload);
        return 1;
    }
    json_t *video = json_object_get(payload, "video");
    if (!video) {
        fprintf(stderr, "error: key not found\n");
        json_decref(video);
        return 1;
    }
    video_value = json_object_get(video, "fps");
    if (!video_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(video_value);
        return 1;
    }
    params->fps = json_number_value(video_value);

    video_value = json_object_get(video, "pixelFormat");
    if (!video_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(video_value);
        return 1;
    }
    char *temp_pix_format = (char *)json_string_value(video_value);
    if (strcmp(temp_pix_format, "yuv422p10le") == 0) {
        params->pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE;
    } else if (strcmp(temp_pix_format, "v210") == 0) {
        params->pixel_format = MESH_VIDEO_PIXEL_FORMAT_V210;
    } else if (strcmp(temp_pix_format, "yuv422p10rfc4175") == 0) {
        params->pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10;
    } else {
        fprintf(stderr, "error: invalid pixel format\n");
        return 1;
    }

    video_value = json_object_get(video, "width");
    if (!video_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(video_value);
        return 1;
    }
    params->width = (int)json_number_value(video_value);

    video_value = json_object_get(video, "height");
    if (!video_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(video_value);
        return 1;
    }
    params->height = (int)json_number_value(video_value);
exit:
    return 0;
}

int get_audio_params(const char *json_string, audio_params *params) {
    json_t *root;
    json_error_t error;
    json_t *audio_value;

    root = json_loads(json_string, 0, &error);
    if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        json_decref(root);
        return 1;
    }

    json_t *payload = json_object_get(root, "payload");
    if (!payload) {
        fprintf(stderr, "error: key not found\n");
        json_decref(payload);
        return 1;
    }

    json_t *audio = json_object_get(payload, "audio");
    if (!audio) {
        fprintf(stderr, "error: key not found\n");
        json_decref(audio);
        return 1;
    }

    audio_value = json_object_get(audio, "sampleRate");
    if (!audio_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(audio_value);
        return 1;
    }
    params->sample_rate = (long int)json_number_value(audio_value);

    audio_value = json_object_get(audio, "channels");
    if (!audio_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(audio_value);
        return 1;
    }
    params->channels = json_number_value(audio_value);

    audio_value = json_object_get(audio, "format");
    if (!audio_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(audio_value);
        return 1;
    }
    params->format = (char *)json_string_value(audio_value);

    audio_value = json_object_get(audio, "packetTime");
    if (!audio_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(audio_value);
        return 1;
    }
    params->packet_time = parse_time_string_to_us(json_string_value(audio_value));

exit:
    return 0;
}

long int parse_time_string_to_us(const char *input) {
    // Check for null input
    if (input == NULL) {
        fprintf(stderr, "Error: Input is NULL\n");
        return -1;
    }

    // Find the position of the first non-digit character
    int i = 0;
    while (isdigit(input[i])) {
        i++;
    }

    // If no digits were found, return an error
    if (i == 0) {
        fprintf(stderr, "Error: No number found in input\n");
        return -1;
    }

    // Extract the number part
    char number_part[i + 1];
    strncpy(number_part, input, i);
    number_part[i] = '\0';

    // Convert the number part to an integer
    int number = atoi(number_part);

    // Extract the suffix part
    const char *suffix = input + i;

    // Convert based on the suffix
    if (strcmp(suffix, "s") == 0) {
        return number * S_TO_US_RATIO; // seconds to microseconds
    } else if (strcmp(suffix, "ms") == 0) {
        return number * MS_TO_US_RATIO; // milliseconds to microseconds
    } else if (strcmp(suffix, "us") == 0) {
        return number; // microseconds
    } else {
        fprintf(stderr, "Error: Invalid suffix '%s'\n", suffix);
        return -1;
    }
}

void parse_cli_commands(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-l", 2) == 0) {
            // Check if the parameter is "-l<number>" or "-li<number>"
            if (argv[i][2] == 'i') {
                input_loop = -1;
            } else {
                // Extract the number following "-l"
                char *number_str = argv[i] + 2; // Skip "-l"
                int number = atoi(number_str);
                input_loop = number; // Set loop to the number
            }
        }
    }
}
