/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LIBFABRIC_MR_H
#define __LIBFABRIC_MR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libfabric_ep.h"
#include "libfabric_dev.h"
#include <rdma/fi_domain.h>

#ifdef UNIT_TESTS_ENABLED
int rdma_reg_mr(libfabric_ctx *rdma_ctx, struct fid_ep *ep, void *buf, size_t size, uint64_t access,
                uint64_t key, enum fi_hmem_iface iface, uint64_t device, struct fid_mr **mr,
                void **desc);
uint64_t rdma_info_to_mr_access(struct fi_info *info);
void rdma_unreg_mr(struct fid_mr *data_mr);
#endif

/**
 * Isolation interface for testability. Accessed from unit tests only.
 */
struct libfabric_mr_ops_t {
    int (*rdma_reg_mr)(libfabric_ctx *rdma_ctx, struct fid_ep *ep, void *buf, size_t size,
                       uint64_t access, uint64_t key, enum fi_hmem_iface iface, uint64_t device,
                       struct fid_mr **mr, void **desc);
    uint64_t (*rdma_info_to_mr_access)(struct fi_info *info);
    void (*rdma_unreg_mr)(struct fid_mr *data_mr);
};

extern struct libfabric_mr_ops_t libfabric_mr_ops;

#ifdef __cplusplus
}
#endif

#endif /* __LIBFABRIC_MR_H */
