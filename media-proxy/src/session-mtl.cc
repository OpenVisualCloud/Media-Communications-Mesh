/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "session-mtl.h"

int frame_available_callback_wrapper(void *priv_data)
{
    mtl_session *connection = (mtl_session *)priv_data;
    return connection->frame_available_cb();
}

int mtl_session::frame_available_cb()
{
    pthread_mutex_lock(&wake_mutex);
    pthread_cond_signal(&wake_cond);
    pthread_mutex_unlock(&wake_mutex);
    return 0;
}

void mtl_session::session_stop()
{
    stop = true;

    pthread_mutex_lock(&wake_mutex);
    pthread_cond_signal(&wake_cond);
    pthread_mutex_unlock(&wake_mutex);
}

mtl_session::mtl_session() : st(0)
{
    pthread_mutex_init(&wake_mutex, NULL);
    pthread_cond_init(&wake_cond, NULL);
    stop = false;
}

mtl_session::~mtl_session()
{
    stop = true;
    pthread_mutex_destroy(&wake_mutex);
    pthread_cond_destroy(&wake_cond);
}

/* Initiliaze MTL library */
mtl_handle inst_init(struct mtl_init_params *st_param)
{
    if (!st_param) {
        return NULL;
    }

    st_param->flags |= MTL_FLAG_RX_UDP_PORT_ONLY;

    // create device
    mtl_handle dev_handle = mtl_init(st_param);
    if (!dev_handle) {
        ERROR("%s, st_init fail\n", __func__);
        return NULL;
    }

    // start MTL device
    if (mtl_start(dev_handle) != 0) {
        INFO("%s, Fail to start MTL device.", __func__);
        return NULL;
    }

    return dev_handle;
}

/* Deinitialize MTL */
void mtl_deinit(mtl_handle dev_handle)
{
    if (dev_handle) {
        // stop tx
        mtl_stop(dev_handle);

        mtl_uninit(dev_handle);
    }
}
