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
        printf("INFO: " __VA_ARGS__); \
        printf("\n");                 \
    } while (0)
#define ERROR(...)                     \
    do {                               \
        printf("ERROR: " __VA_ARGS__); \
        printf("\n");                  \
    } while (0)

#define DEBUG(...) \
    do {           \
    } while (0)

#endif