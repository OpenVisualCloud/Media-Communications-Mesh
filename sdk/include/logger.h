/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>

#define logmsg(tag, fmt, ...)                         \
    do {                                              \
        printf("%s - " fmt "\n", tag, ##__VA_ARGS__); \
    } while (0)

#define log_info(fmt, ...)                  \
    do {                                    \
        logmsg("INFO", fmt, ##__VA_ARGS__); \
    } while (0)

#ifdef DEBUG
  #define log_debug(fmt, ...)                  \
      do {                                     \
          logmsg("DEBUG", fmt, ##__VA_ARGS__); \
      } while (0)
#else
  #define log_debug(fmt, ...)  // Define log_debug as empty in release build
#endif

#define log_warn(fmt, ...)                  \
    do {                                    \
        logmsg("WARN", fmt, ##__VA_ARGS__); \
    } while (0)

#define log_error(fmt, ...)                  \
    do {                                     \
        logmsg("ERROR", fmt, ##__VA_ARGS__); \
    } while (0)

#endif /* LOG_H */