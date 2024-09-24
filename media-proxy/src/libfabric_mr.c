/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Copyright (c) 2013-2018 Intel Corporation.  All rights reserved.
 * Copyright (c) 2016 Cray Inc.  All rights reserved.
 * Copyright (c) 2014-2017, Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates. All rights reserved.
 *
 * This software is available to you under the BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>

#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_collective.h>

#include "libfabric_mr.h"
#include "rdma_hmem.h"

static void rdma_fill_mr_attr(struct iovec *iov, struct fi_mr_dmabuf *dmabuf, int iov_count,
                              uint64_t access, uint64_t key, enum fi_hmem_iface iface,
                              uint64_t device, struct fi_mr_attr *attr, uint64_t flags)
{
    if (flags & FI_MR_DMABUF)
        attr->dmabuf = dmabuf;
    else
        attr->mr_iov = iov;
    attr->iov_count = iov_count;
    attr->access = access;
    attr->offset = 0;
    attr->requested_key = key;
    attr->context = NULL;
    attr->iface = iface;

    switch (iface) {
    case FI_HMEM_NEURON:
        attr->device.neuron = device;
        break;
    case FI_HMEM_ZE:
        attr->device.ze = fi_hmem_ze_device(0, device);
        break;
    case FI_HMEM_CUDA:
        attr->device.cuda = device;
        break;
    default:
        break;
    }
}

int rdma_reg_mr(libfabric_ctx *rdma_ctx, struct fid_ep *ep, struct fi_info *fi, void *buf,
                size_t size, uint64_t access, uint64_t key, enum fi_hmem_iface iface,
                uint64_t device, struct fid_mr **mr, void **desc)
{
    struct fi_mr_dmabuf dmabuf = { 0 };
    struct fi_mr_attr attr = { 0 };
    struct iovec iov = { 0 };
    uint64_t dmabuf_offset;
    uint64_t flags;
    int dmabuf_fd;
    int ret;

    iov.iov_base = buf;
    iov.iov_len = size;

    flags = (iface) ? FI_HMEM_DEVICE_ONLY : 0;

    rdma_fill_mr_attr(&iov, &dmabuf, 1, access, key, iface, device, &attr, flags);
    ret = fi_mr_regattr(rdma_ctx->domain, &attr, flags, mr);
    if (ret)
        return ret;

    if (desc)
        *desc = fi_mr_desc(*mr);

    if (rdma_ctx->info->domain_attr->mr_mode & FI_MR_ENDPOINT) {
        ret = fi_mr_bind(*mr, &ep->fid, 0);
        if (ret)
            return ret;

        ret = fi_mr_enable(*mr);
        if (ret)
            return ret;
    }

    return 0;
}

static inline int rdma_rma_write_target_allowed(uint64_t caps)
{
    if (caps & (FI_RMA | FI_ATOMIC)) {
        if (caps & FI_REMOTE_WRITE)
            return 1;
        return !(caps & (FI_READ | FI_WRITE | FI_REMOTE_WRITE));
    }
    return 0;
}

static inline int rdma_rma_read_target_allowed(uint64_t caps)
{
    if (caps & (FI_RMA | FI_ATOMIC)) {
        if (caps & FI_REMOTE_READ)
            return 1;
        return !(caps & (FI_READ | FI_WRITE | FI_REMOTE_WRITE));
    }
    return 0;
}

static inline int rdma_check_mr_local_flag(struct fi_info *info)
{
    return ((info->mode & FI_LOCAL_MR) || (info->domain_attr->mr_mode & FI_MR_LOCAL));
}

uint64_t rdma_info_to_mr_access(struct fi_info *info)
{
    uint64_t mr_access = 0;
    if (rdma_check_mr_local_flag(info)) {
        if (info->caps & (FI_MSG | FI_TAGGED)) {
            if (info->caps & (FI_SEND | FI_RECV)) {
                mr_access |= info->caps & (FI_SEND | FI_RECV);
            } else {
                mr_access |= (FI_SEND | FI_RECV);
            }
        }

        if (info->caps & (FI_RMA | FI_ATOMIC)) {
            if (info->caps & (FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE)) {
                mr_access |= info->caps & (FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE);
            } else {
                mr_access |= (FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE);
            }
        }
    } else {
        if (info->caps & (FI_RMA | FI_ATOMIC)) {
            if (rdma_rma_read_target_allowed(info->caps)) {
                mr_access |= FI_REMOTE_READ;
            }
            if (rdma_rma_write_target_allowed(info->caps)) {
                mr_access |= FI_REMOTE_WRITE;
            }
        }
    }
    return mr_access;
}
