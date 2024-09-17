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
    char *data_buf;
    size_t data_buf_size;
    int rx_fd, tx_fd;
    struct fid_cq *txcq, *rxcq;
    struct fid_av *av;
    struct fid_mr *data_mr;
    void *data_desc;
    fi_addr_t dest_av_entry;

    struct fid_wait *waitset;
    struct fi_context *recv_ctx;
    struct fi_context *send_ctx;
    uint64_t tx_cq_cntr;
    uint64_t rx_cq_cntr;

    libfabric_ctx *rdma_ctx;
} ep_ctx_t;

typedef struct {
    libfabric_ctx *rdma_ctx;
    char *data_buf;
    size_t data_buf_size;
    rdma_addr remote_addr;
    rdma_addr local_addr;
    enum direction dir;
} ep_cfg_t;

int ep_send_buf(ep_ctx_t *ep_ctx, char *buf, size_t buf_size);
// *buf has to point to registered memory
int ep_recv_buf(ep_ctx_t *ep_ctx, char *buf, size_t buf_size);
int ep_init(ep_ctx_t **ep_ctx, ep_cfg_t *cfg);
int ep_destroy(ep_ctx_t **ep_ctx);

#ifdef __cplusplus
}
#endif

#endif /* __LIBFABRIC_EP_H */
