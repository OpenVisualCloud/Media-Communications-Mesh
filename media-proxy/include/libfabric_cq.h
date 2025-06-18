/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LIBFABRIC_CQ_H
#define __LIBFABRIC_CQ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rdma/fi_eq.h>
#include <stdbool.h>

/* forward declaration */
typedef struct ep_ctx_t ep_ctx_t;

enum cq_comp_method {
    RDMA_COMP_SPIN = 0,
    RDMA_COMP_SREAD,
    RDMA_COMP_WAITSET,
    RDMA_COMP_WAIT_FD,
    RDMA_COMP_YIELD,
};

typedef struct {
    struct fid_cq *cq;
    struct fid_wait *waitset;
    uint64_t cq_cntr;
    int cq_fd;
    int (*eq_read)(ep_ctx_t *ep_ctx, struct fi_cq_err_entry *entry, int timeout);
    bool external;
} cq_ctx_t;

#ifdef UNIT_TESTS_ENABLED
int rdma_cq_open(ep_ctx_t *ep_ctx, size_t cq_size, enum cq_comp_method comp_method);
int rdma_read_cq(ep_ctx_t *ep_ctx, struct fi_cq_err_entry *entry, int timeout);
int rdma_cq_readerr(struct fid_cq *cq);
#endif /* UNIT_TESTS_ENABLED */

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct libfabric_cq_ops_t {
    int (*rdma_read_cq)(struct ep_ctx_t *ep_ctx, struct fi_cq_err_entry *entry, int timeout);
    int (*rdma_cq_readerr)(struct fid_cq *cq);
    int (*rdma_cq_open)(ep_ctx_t *ep_ctx, size_t cq_size, enum cq_comp_method comp_method);
};

extern struct libfabric_cq_ops_t libfabric_cq_ops;

#ifdef __cplusplus
}
#endif

#endif /* __LIBFABRIC_CQ_H */
