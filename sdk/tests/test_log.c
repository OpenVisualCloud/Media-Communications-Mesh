/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <time.h>
#include <unistd.h>

#include "logger.h"

int main(int argc, char **argv)
{
    log_info("Normal information.");
    log_warn("Warning.");
    log_debug("Debug log.");
    log_error("Error message.");

    return 0;
}