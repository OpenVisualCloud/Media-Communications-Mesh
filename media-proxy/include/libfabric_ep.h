/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LIBFABRIC_EP_H
#define __LIBFABRIC_EP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rdma/fabric.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>
#include "libfabric_dev.h"
#include "utils.h"

typedef struct {
    char ip[46];
    char port[6];
} rdma_addr;

typedef struct {
    struct fid_ep *ep;
    int rx_fd, tx_fd;
    struct fid_cq *txcq, *rxcq;
    struct fid_av *av;
    struct fid_mr *data_mr;
    uint64_t mr_access;
    void *data_desc;
    fi_addr_t dest_av_entry;

    struct fid_wait *waitset;
    uint64_t tx_cq_cntr;
    uint64_t rx_cq_cntr;

    libfabric_ctx *rdma_ctx;
} ep_ctx_t;

typedef struct {
    libfabric_ctx *rdma_ctx;
    rdma_addr remote_addr;
    rdma_addr local_addr;
    enum direction dir;
} ep_cfg_t;

int ep_send_buf(ep_ctx_t *ep_ctx, void *buf, size_t buf_size);
// *buf has to point to registered memory
int ep_recv_buf(ep_ctx_t *ep_ctx, void *buf, size_t buf_size, void *buf_ctx);
int ep_rxcq_read(ep_ctx_t *ep_ctx, void **buf_ctx, int timeout);
int ep_txcq_read(ep_ctx_t *ep_ctx, int timeout);
int ep_reg_mr(ep_ctx_t *ep_ctx, void *data_buf, size_t data_buf_size);
int ep_init(ep_ctx_t **ep_ctx, ep_cfg_t *cfg);
int ep_destroy(ep_ctx_t **ep_ctx);

#ifdef __cplusplus
}
#endif

#endif /* __LIBFABRIC_EP_H */
