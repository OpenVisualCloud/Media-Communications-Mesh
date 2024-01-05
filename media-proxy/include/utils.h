/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>

#define INFO(...)                     \
    do {                              \
        fprintf(stdout, "INFO: " __VA_ARGS__); \
        fprintf(stdout, "\n");                 \
    } while (0)

#define ERROR(...)                     \
    do {                               \
        fprintf(stderr, "ERROR: " __VA_ARGS__); \
        fprintf(stderr, "\n");                  \
    } while (0)

#define DEBUG(...) \
    do {           \
    } while (0)

#endif