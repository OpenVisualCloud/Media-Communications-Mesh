/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SESSIONS_H
#define __SESSIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mcm_dp.h"
#include "mtl.h"
#include "libfabric_dev.h"
#include "rdma_session.h"

typedef struct {
    uint32_t id;
    enum direction type;
    mcm_payload_type payload_type;
    union {
        tx_session_context_t *tx_session;
        rx_session_context_t *rx_session;
        tx_st22p_session_context_t *tx_st22p_session;
        rx_st22p_session_context_t *rx_st22p_session;
        tx_st30_session_context_t *tx_st30_session;
        rx_st30_session_context_t *rx_st30_session;
        tx_st40_session_context_t *tx_st40_session;
        rx_st40_session_context_t *rx_st40_session;
        tx_rdma_session_context_t *tx_rdma_session;
        rx_rdma_session_context_t *rx_rdma_session;
    };
} dp_session_context_t;

#ifdef __cplusplus
}
#endif

#endif /* __SESSIONS_H */
