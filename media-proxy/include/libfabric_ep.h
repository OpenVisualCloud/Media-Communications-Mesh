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
#include "libfabric_cq.h"
#include "utils.h"

typedef struct {
    char ip[46];
    char port[6];
} rdma_addr;

typedef struct ep_ctx_t {
    struct fid_ep *ep;

    struct fid_av *av;
    struct fid_mr *data_mr;
    void *data_desc;
    fi_addr_t dest_av_entry;

    cq_ctx_t cq_ctx;

    libfabric_ctx *rdma_ctx;

    volatile bool stop_flag;
} ep_ctx_t;

typedef struct {
    libfabric_ctx *rdma_ctx;
    rdma_addr remote_addr;
    rdma_addr local_addr;
    enum direction dir;
} ep_cfg_t;

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct libfabric_ep_ops_t {
    int (*ep_reg_mr)(ep_ctx_t *ep_ctx, void *data_buf, size_t data_buf_size);
    int (*ep_send_buf)(ep_ctx_t *ep_ctx, void *buf, size_t buf_size);
    int (*ep_recv_buf)(ep_ctx_t *ep_ctx, void *buf, size_t buf_size, void *buf_ctx);
    int (*ep_cq_read)(ep_ctx_t *ep_ctx, void **buf_ctx, int timeout);
    int (*ep_init)(ep_ctx_t **ep_ctx, ep_cfg_t *cfg);
    int (*ep_destroy)(ep_ctx_t **ep_ctx);
};

extern struct libfabric_ep_ops_t libfabric_ep_ops;

#ifdef UNIT_TESTS_ENABLED
/* buf has to point to memory registered with ep_reg_mr */
int ep_send_buf(ep_ctx_t *ep_ctx, void *buf, size_t buf_size);
int ep_recv_buf(ep_ctx_t *ep_ctx, void *buf, size_t buf_size, void *buf_ctx);
int ep_cq_read(ep_ctx_t *ep_ctx, void **buf_ctx, int timeout);
int ep_reg_mr(ep_ctx_t *ep_ctx, void *data_buf, size_t data_buf_size);
int ep_init(ep_ctx_t **ep_ctx, ep_cfg_t *cfg);
int ep_destroy(ep_ctx_t **ep_ctx);
#endif /* UNIT_TESTS_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* __LIBFABRIC_EP_H */
