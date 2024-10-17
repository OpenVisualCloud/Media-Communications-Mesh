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

#include "libfabric_ep.h"

void rdma_cq_set_wait_attr(struct fi_cq_attr *cq_attr, enum cq_comp_method method,
                           struct fid_wait *waitset);
int rdma_read_cq(ep_ctx_t *ep_ctx, struct fid_cq *cq, uint64_t *cur, uint64_t total, int timeout,
                 struct fi_cq_err_entry *entries);

int rdma_get_cq_fd(struct fid_cq *cq, int *fd, enum cq_comp_method method);
int rdma_get_cq_comp(ep_ctx_t *ep_ctx, struct fid_cq *cq, uint64_t *cur, uint64_t total,
                     int timeout, struct fi_cq_err_entry *entry);
int rdma_cq_readerr(struct fid_cq *cq);

#ifdef __cplusplus
}
#endif

#endif /* __LIBFABRIC_CQ_H */
