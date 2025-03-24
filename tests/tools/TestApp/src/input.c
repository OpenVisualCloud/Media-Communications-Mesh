/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "input.h"
#include <jansson.h>

char *input_parse_json_to_string(const char *file_name) {
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



video_params get_video_params(const char *json_string){

  json_t *root;
  json_error_t error;
  json_t* video_value;

    video_params params ={};

  root = json_loads(json_string, 0, &error);
  if (!root) {
      fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
      json_decref(root);
      goto exit;
  }

  json_t* payload = json_object_get(root,"payload");
  if (!payload) {
    fprintf(stderr, "error: key not found\n");
    json_decref(payload);
    goto exit;
  }
  json_t* video = json_object_get(payload,"video");
  if (!video) {
      fprintf(stderr, "error: key not found\n");
      json_decref(video);
      goto exit;
  }
  video_value = json_object_get(video,"fps");
  if (!video_value) {
      fprintf(stderr, "error: key not found\n");
      json_decref(video_value);
      goto exit;
  }
  params.fps = (int)json_number_value(video_value);

  video_value = json_object_get(video,"pixelFormat");
  if (!video_value) {
      fprintf(stderr, "error: key not found\n");
      json_decref(video_value);
      goto exit;
  }
  params.pixel_format = json_string_value(video_value);

  video_value = json_object_get(video,"width");
  if (!video_value) {
      fprintf(stderr, "error: key not found\n");
      json_decref(video_value);
      goto exit;
  }
    params.width = (int)json_number_value(video_value);

  video_value = json_object_get(video,"height");
  if (!video_value) {
      fprintf(stderr, "error: key not found\n");
      json_decref(video_value);
      goto exit;
  }
  params.height = (int)json_number_value(video_value);
exit:
    return params;
}

audio_params get_audio_params(const char *json_string) {
    json_t *root;
    json_error_t error;
    json_t *audio_value;

    audio_params params = {};

    root = json_loads(json_string, 0, &error);
    if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        json_decref(root);
        goto exit;
    }

    json_t *payload = json_object_get(root, "payload");
    if (!payload) {
        fprintf(stderr, "error: key not found\n");
        json_decref(payload);
        goto exit;
    }

    json_t *audio = json_object_get(payload, "audio");
    if (!audio) {
        fprintf(stderr, "error: key not found\n");
        json_decref(audio);
        goto exit;
    }

    audio_value = json_object_get(audio, "sampleRate");
    if (!audio_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(audio_value);
        goto exit;
    }
    params.sample_rate = (int)json_number_value(audio_value);

    audio_value = json_object_get(audio, "channels");
    if (!audio_value) {
        fprintf(stderr, "error: key not found\n");
        json_decref(audio_value);
        goto exit;
    }
    params.channels = (int)json_number_value(audio_value);

exit:
    return params;
}