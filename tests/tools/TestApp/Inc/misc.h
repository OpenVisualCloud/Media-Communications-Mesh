/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _MISC_H_
#define _MISC_H_

#define SHUTDOWN_REQUESTED 1

void LOG(const char *format, ...);
void setup_sig_int();
extern volatile int shutdown_flag;

#endif /* _MISC_H_ */
