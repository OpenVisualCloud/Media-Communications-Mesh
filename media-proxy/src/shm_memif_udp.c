/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>

#include "mtl.h"
#include "shm_memif.h"

int rx_udp_h264_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    rx_udp_h264_session_context_t* rx_ctx = (rx_udp_h264_session_context_t*)priv_data;
    int err = 0;

    INFO("RX memif connected!");

    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    rx_ctx->fb_count = md.rx_queues[0].ring_size;

    free(buf);

    /* rx buffers */
    rx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * rx_ctx->fb_count);
    if (!rx_ctx->shm_bufs) {
        ERROR("Failed to allocate memory");
        return -ENOMEM;
    }
    rx_ctx->shm_buf_num = rx_ctx->fb_count;

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    rx_ctx->shm_ready = 1;

    return 0;
}
