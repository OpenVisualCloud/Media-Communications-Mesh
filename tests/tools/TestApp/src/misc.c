/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "misc.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

void LOG(const char *format, ...) {
    time_t rawtime;
    struct tm *timeinfo;
    char time_buffer[20];
    struct timespec ts;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    clock_gettime(CLOCK_REALTIME, &ts);

    strftime(time_buffer, sizeof(time_buffer), "%b %d %H:%M:%S", timeinfo);

    printf("%s.%03ld  ", time_buffer, ts.tv_nsec / 1000000);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}
