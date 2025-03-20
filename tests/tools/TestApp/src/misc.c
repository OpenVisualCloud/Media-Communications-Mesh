/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "misc.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>

int shutdown_flag = 0;

struct sigaction sa_int;
struct sigaction sa_term;

void sig_handler(int sig);
void setup_signal_handler(struct sigaction *sa, void (*handler)(int),int sig);

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


void setup_sig_int(){
    static struct sigaction sa_int;
    static struct sigaction sa_term;
    setup_signal_handler(&sa_int, sig_handler, SIGINT);
    setup_signal_handler(&sa_term, sig_handler, SIGTERM);
}


void sig_handler(int sig) {
    shutdown_flag = SHUTDOWN_REQUESTED;
}

void setup_signal_handler(struct sigaction *sa, void (*handler)(int),int sig) {
    sa->sa_handler = handler;
    sigemptyset(&(sa->sa_mask));
    sa->sa_flags = 0;
    sigaction(sig, sa, NULL);
}