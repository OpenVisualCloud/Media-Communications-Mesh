/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "session-mtl.h"

int frame_available_callback_wrapper(void *priv)
{
    if (!priv) {
        return -1;
    }
    MtlSession *connection = (MtlSession *)priv;
    return connection->frame_available_cb();
}

int MtlSession::frame_available_cb()
{
    pthread_mutex_lock(&wake_mutex);
    pthread_cond_signal(&wake_cond);
    pthread_mutex_unlock(&wake_mutex);
    return 0;
}

MtlSession::MtlSession(memif_ops_t &memif_ops, mcm_payload_type payload, direction dir_type,
                       mtl_handle st)
    : Session(memif_ops, payload, dir_type), st(st), stop(false)
{
    pthread_mutex_init(&wake_mutex, NULL);
    pthread_cond_init(&wake_cond, NULL);
}

MtlSession::~MtlSession()
{
    stop = true;

    pthread_mutex_lock(&wake_mutex);
    pthread_cond_signal(&wake_cond);
    pthread_mutex_unlock(&wake_mutex);

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

st_frame_fmt get_st_frame_fmt(video_pixel_format mcm_frame_fmt)
{
    st_frame_fmt mtl_frame_fmt;
    switch (mcm_frame_fmt) {
    case PIX_FMT_V210:
        mtl_frame_fmt = ST_FRAME_FMT_V210;
        break;
    case PIX_FMT_YUV422RFC4175BE10:
        mtl_frame_fmt = ST_FRAME_FMT_YUV422RFC4175PG2BE10;
        break;
    case PIX_FMT_YUV422PLANAR10LE:
    default:
        mtl_frame_fmt = ST_FRAME_FMT_YUV422PLANAR10LE;
    }
    return mtl_frame_fmt;
}
