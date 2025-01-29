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

char *parse_json_to_string(const char *file_name) {
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
