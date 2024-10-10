/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _ST_APP_PLATFORM_HEAD_H_
#define _ST_APP_PLATFORM_HEAD_H_

#include <signal.h>

enum st_tx_frame_status {
    ST_TX_FRAME_FREE = 0,
    ST_TX_FRAME_READY,
    ST_TX_FRAME_IN_TRANSMITTING,
    ST_TX_FRAME_STATUS_MAX,
};

struct st_tx_frame {
    enum st_tx_frame_status stat;
    size_t size;
};

static inline int st_pthread_mutex_init(pthread_mutex_t* mutex,
    pthread_mutexattr_t* attr)
{
    return pthread_mutex_init(mutex, attr);
}

static inline int st_pthread_mutex_lock(pthread_mutex_t* mutex)
{
    return pthread_mutex_lock(mutex);
}

static inline int st_pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    return pthread_mutex_unlock(mutex);
}

static inline int st_pthread_mutex_destroy(pthread_mutex_t* mutex)
{
    return pthread_mutex_destroy(mutex);
}

static inline int st_pthread_cond_init(pthread_cond_t* cond,
    pthread_condattr_t* cond_attr)
{
    return pthread_cond_init(cond, cond_attr);
}

static inline int st_pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    return pthread_cond_wait(cond, mutex);
}

static inline int st_pthread_cond_destroy(pthread_cond_t* cond)
{
    return pthread_cond_destroy(cond);
}

static inline int st_pthread_cond_signal(pthread_cond_t* cond)
{
    return pthread_cond_signal(cond);
}

#endif
