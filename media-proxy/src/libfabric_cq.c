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
#include <stdio.h>

#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_collective.h>

#include "libfabric_cq.h"
#include "libfabric_dev.h"
#include "libfabric_ep.h"

#define CQ_TIMEOUT (-1)

static void rdma_cq_set_wait_attr(struct fi_cq_attr *cq_attr, enum cq_comp_method method,
                                  struct fid_wait *waitset)
{
    switch (method) {
    case RDMA_COMP_SREAD:
        cq_attr->wait_obj = FI_WAIT_UNSPEC;
        cq_attr->wait_cond = FI_CQ_COND_NONE;
        break;
    case RDMA_COMP_WAITSET:
        assert(waitset);
        cq_attr->wait_obj = FI_WAIT_SET;
        cq_attr->wait_cond = FI_CQ_COND_NONE;
        cq_attr->wait_set = waitset;
        break;
    case RDMA_COMP_WAIT_FD:
        cq_attr->wait_obj = FI_WAIT_FD;
        cq_attr->wait_cond = FI_CQ_COND_NONE;
        break;
    case RDMA_COMP_YIELD:
        cq_attr->wait_obj = FI_WAIT_YIELD;
        cq_attr->wait_cond = FI_CQ_COND_NONE;
        break;
    default:
        cq_attr->wait_obj = FI_WAIT_NONE;
        break;
    }
}

static int rdma_spin_for_comp(struct ep_ctx_t *ep_ctx, struct fi_cq_err_entry *entry, int timeout)
{
    struct timespec a, b;
    int ret;

    if (timeout >= 0)
        clock_gettime(CLOCK_MONOTONIC, &a);

    do {
        ret = fi_cq_read(ep_ctx->cq_ctx.cq, entry, 1);
        if (ret < 0 && ret != -FI_EAGAIN) {
            return ret;
        } else if (ret == -FI_EAGAIN && timeout >= 0) {
            clock_gettime(CLOCK_MONOTONIC, &b);
            if ((b.tv_sec - a.tv_sec) > timeout) {
                fprintf(stderr, "%ds timeout expired\n", timeout);
                return -ENODATA;
            }
        }
    } while (ret <= 0);

    ep_ctx->cq_ctx.cq_cntr++;

    return 0;
}

static int rdma_poll_fd(int fd, int timeout)
{
    struct pollfd fds;
    int ret;

    fds.fd = fd;
    fds.events = POLLIN;
    ret = poll(&fds, 1, timeout);
    if (ret == -1) {
        RDMA_PRINTERR("poll", -errno);
        ret = -errno;
    } else if (!ret) {
        RDMA_WARN("poll timed out");
        ret = -EAGAIN;
    } else {
        ret = 0;
    }
    return ret;
}

static int rdma_fdwait_for_comp(struct ep_ctx_t *ep_ctx, struct fi_cq_err_entry *entry, int timeout)
{
    struct fid *fids[1];
    int ret;

    fids[0] = &ep_ctx->cq_ctx.cq->fid;

    ret = fi_trywait(ep_ctx->rdma_ctx->fabric, fids, 1);
    if (!ret) {
        ret = rdma_poll_fd(ep_ctx->cq_ctx.cq_fd, timeout);
        if (ret)
            return ret;
    }

    ret = fi_cq_read(ep_ctx->cq_ctx.cq, entry, 1);
    if (ret > 0) {
        ep_ctx->cq_ctx.cq_cntr++;
    } else if (ret < 0) {
        return ret;
    }

    return 0;
}

static int rdma_wait_for_comp(struct ep_ctx_t *ep_ctx, struct fi_cq_err_entry *entry, int timeout)
{
    int ret;

    ret = fi_cq_sread(ep_ctx->cq_ctx.cq, entry, 1, NULL, timeout);
    if (ret > 0) {
        ep_ctx->cq_ctx.cq_cntr++;
    } else if (ret < 0) {
        return ret;
    }

    return 0;
}

int rdma_read_cq(struct ep_ctx_t *ep_ctx, struct fi_cq_err_entry *entry, int timeout)
{
    int ret;

    ret = ep_ctx->cq_ctx.eq_read(ep_ctx, entry, timeout);

    if (ret) {
        if (ret == -FI_EAVAIL) {
            ret = rdma_cq_readerr(ep_ctx->cq_ctx.cq);
            ep_ctx->cq_ctx.cq_cntr++;
        } else if ( ret != -FI_EAGAIN) {
            RDMA_PRINTERR("rdma_get_cq_comp", ret);
        }
    }
    return ret;
}

int rdma_cq_readerr(struct fid_cq *cq)
{
    struct fi_cq_err_entry cq_err = { 0 };
    int ret;

    ret = fi_cq_readerr(cq, &cq_err, 0);
    if (ret < 0) {
        RDMA_PRINTERR("fi_cq_readerr", ret);
    } else {
        RDMA_CQ_ERR(cq, cq_err, NULL, 0);
        ret = -cq_err.err;
    }
    return ret;
}

int rdma_cq_open(ep_ctx_t *ep_ctx, size_t cq_size, enum cq_comp_method comp_method)
{
    struct fi_cq_attr cq_attr = {.wait_obj = FI_WAIT_NONE, .format = FI_CQ_FORMAT_CONTEXT};
    int err;

    rdma_cq_set_wait_attr(&cq_attr, comp_method, NULL);
    if (cq_size)
        cq_attr.size = cq_size;
    else
        cq_attr.size = ep_ctx->rdma_ctx->info->tx_attr->size;

    err = fi_cq_open(ep_ctx->rdma_ctx->domain, &cq_attr, &ep_ctx->cq_ctx.cq, ep_ctx);
    if (err) {
        RDMA_PRINTERR("fi_cq_open", err);
        return err;
    }

    if (comp_method == RDMA_COMP_WAIT_FD) {
        err = fi_control(&ep_ctx->cq_ctx.cq->fid, FI_GETWAIT, &ep_ctx->cq_ctx.cq_fd);
        if (err)
            RDMA_PRINTERR("fi_control(FI_GETWAIT)", err);
    }

    switch (comp_method) {
    case RDMA_COMP_SREAD:
    case RDMA_COMP_YIELD:
        ep_ctx->cq_ctx.eq_read = rdma_wait_for_comp;
        break;
    case RDMA_COMP_WAIT_FD:
        ep_ctx->cq_ctx.eq_read = rdma_fdwait_for_comp;
        break;
    default:
        ep_ctx->cq_ctx.eq_read = rdma_spin_for_comp;
        break;
    }

    return err;
}

// EQ not CQ
void eq_readerr(struct fid_eq *eq, const char *eq_str)
{
    struct fi_eq_err_entry eq_err = { 0 };
    int rd;

    rd = fi_eq_readerr(eq, &eq_err, 0);
    if (rd != sizeof(eq_err)) {
        RDMA_PRINTERR("fi_eq_readerr", rd);
    } else {
        RDMA_EQ_ERR(eq, eq_err, NULL, 0);
    }
}
