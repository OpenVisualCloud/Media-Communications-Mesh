/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __UTILS_H
#define __UTILS_H

#ifndef MCM_LOG_LEVEL
#define MCM_LOG_LEVEL 16
#endif

#include <stdio.h>

static void log_default_prefix(char* buf, size_t sz) {
  time_t now;
  struct tm tm;

  time(&now);
  localtime_r(&now, &tm);
  strftime(buf, sz, "%Y-%m-%d %H:%M:%S, ", &tm);
}

#define MCM_LOG(l, format, ...)                                                \
  do {                                                                         \
      char __prefix[64];                                                       \
      log_default_prefix(__prefix, sizeof(__prefix));                          \
      printf("MCM %s: %s" format, l, __prefix, ##__VA_ARGS__);                 \
      printf("\n");                                                            \
  } while (0)

// MCM_LOG_LEVEL>=32
#if (MCM_LOG_LEVEL >= 32)
    #define DEBUG(...) do { MCM_LOG("DEBUG", __VA_ARGS__); } while (0)
#else
    #define DEBUG(...) do { } while (0)
#endif

// MCM_LOG_LEVEL>=16
#if (MCM_LOG_LEVEL >= 16)
    #define INFO(...) do { MCM_LOG("INFO", __VA_ARGS__); } while (0)
#else
    #define INFO(...) do { } while (0)
#endif

// MCM_LOG_LEVEL>=8
#if (MCM_LOG_LEVEL >= 8)
    #define NOTICE(...) do { MCM_LOG("NOTICE", __VA_ARGS__); } while (0)
#else
    #define NOTICE(...) do { } while (0)
#endif

// MCM_LOG_LEVEL>=4
#if (MCM_LOG_LEVEL >= 4)
    #define WARNING(...) do { MCM_LOG("WARNING", __VA_ARGS__); } while (0)
#else
    #define WARNING(...) do { } while (0)
#endif

// MCM_LOG_LEVEL>=2
#if (MCM_LOG_LEVEL >= 2)
    #define ERROR(...) do { MCM_LOG("ERROR", __VA_ARGS__); } while (0)
#else
    #define ERROR(...) do { } while (0)
#endif

// MCM_LOG_LEVEL>0
#if (MCM_LOG_LEVEL > 0)
    #define CRITICAL(...) do { MCM_LOG("CRITICAL", __VA_ARGS__); } while (0)
#else
    #define ERROR(...) do { } while (0)
#endif

enum direction {
    TX,
    RX
};

#endif
