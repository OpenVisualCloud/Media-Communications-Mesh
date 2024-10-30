/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SHM_MEMIF_H
#define __SHM_MEMIF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libmemif.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

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

typedef struct {
    uint8_t is_master;
    char app_name[32];
    char interface_name[32];
    uint32_t interface_id;
    char socket_path[108];
    uint32_t m_session_count;
} memif_ops_t;

void print_memif_details(memif_conn_handle_t conn);

int memif_buffer_alloc_timeout(memif_conn_handle_t conn, uint16_t qid,
                               memif_buffer_t * bufs, uint16_t count, uint16_t * count_out,
                               uint32_t size, uint32_t timeout_ms);
int memif_get_buffs_region(memif_conn_handle_t conn, memif_region_details_t *region);

#ifdef __cplusplus
}
#endif

#endif /* __SHM_MEMIF_H */
