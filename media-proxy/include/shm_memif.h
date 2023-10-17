/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SHM_MEMIF_H
#define __SHM_MEMIF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libmemif.h>

#include "mtl.h"
#include "utils.h"

typedef struct {
    uint8_t is_master;
    uint8_t is_connected;
    uint16_t index;

    /* memif conenction handle */
    memif_conn_handle_t conn;

    /* transmit queue id */
    uint16_t qid;

    /* tx buffers */
    memif_buffer_t* tx_bufs;
    /* allocated tx buffers counter */
    /* number of tx buffers pointing to shared memory */
    uint16_t tx_buf_num;

    /* rx buffers */
    memif_buffer_t* rx_bufs;
    /* allcoated rx buffers counter */
    /* number of rx buffers pointing to shared memory */
    uint16_t rx_buf_num;
} shm_connection_t;

/* informs user about connected status. private_ctx is used by user to identify
 * connection */
int rx_on_connect(memif_conn_handle_t conn, void* priv_data);
int rx_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid);

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int rx_on_disconnect(memif_conn_handle_t conn, void* priv_data);

int rx_st22p_on_connect(memif_conn_handle_t conn, void* priv_data);
int rx_st22p_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid);
int rx_st30_on_connect(memif_conn_handle_t conn, void* priv_data);
int rx_st30_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid);
int rx_st40_on_connect(memif_conn_handle_t conn, void* priv_data);
int rx_st40_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid);
int rx_udp_h264_on_connect(memif_conn_handle_t conn, void* priv_data);

int tx_on_connect(memif_conn_handle_t conn, void* priv_data);
int tx_on_disconnect(memif_conn_handle_t conn, void* priv_data);
int tx_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid);
int tx_st22p_on_connect(memif_conn_handle_t conn, void* priv_data);
int tx_st22p_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid);
int tx_st30_on_connect(memif_conn_handle_t conn, void* priv_data);
int tx_st30_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid);
int tx_st40_on_connect(memif_conn_handle_t conn, void* priv_data);
int tx_st40_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid);

#ifdef __cplusplus
}
#endif

#endif /* __SHM_MEMIF_H */
