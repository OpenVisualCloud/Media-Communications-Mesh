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

#include "libfabric_dev.h"

int rdma_reg_mr(libfabric_ctx *rdma_ctx, struct fid_ep *ep, void *buf, size_t size, uint64_t access,
                uint64_t key, enum fi_hmem_iface iface, uint64_t device, struct fid_mr **mr,
                void **desc);

uint64_t rdma_info_to_mr_access(struct fi_info *info);

#ifdef __cplusplus
}
#endif

#endif /* __LIBFABRIC_MR_H */
