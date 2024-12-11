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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <netinet/in.h> // For struct sockaddr_in

#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_collective.h>

#include "libfabric_ep.h"
#include "libfabric_cq.h"
#include "libfabric_mr.h"

static int enable_ep(ep_ctx_t *ep_ctx)
{
    int ret;

    RDMA_EP_BIND(ep_ctx->ep, ep_ctx->av, 0);

    /* TODO: Find out if unidirectional endpoint can be created */
    RDMA_EP_BIND(ep_ctx->ep, ep_ctx->cq_ctx.cq, FI_SEND | FI_RECV);

    ret = fi_enable(ep_ctx->ep);
    if (ret) {
        RDMA_PRINTERR("fi_enable", ret);
    }

    return ret;
}

static int ep_av_insert(libfabric_ctx *rdma_ctx, struct fid_av *av, void *addr, size_t count,
                        fi_addr_t *fi_addr, uint64_t flags, void *context)
{
    int ret;

    ret = fi_av_insert(av, addr, count, fi_addr, flags, context);
    if (ret < 0) {
        RDMA_PRINTERR("fi_av_insert", ret);
        return ret;
    } else if (ret != count) {
        RDMA_ERR("fi_av_insert: number of addresses inserted = %d;"
                 " number of addresses given = %zd\n",
                 ret, count);
        return -EINVAL;
    }

    return 0;
}

static int ep_alloc_res(ep_ctx_t *ep_ctx, libfabric_ctx *rdma_ctx, struct fi_info *fi,
                        size_t av_size)
{
    struct fi_av_attr av_attr = {.type = FI_AV_MAP, .count = 1};
    int ret;

    ret = fi_endpoint(rdma_ctx->domain, fi, &ep_ctx->ep, NULL);
    if (ret) {
        RDMA_PRINTERR("fi_endpoint", ret);
        return ret;
    }

    ret = libfabric_cq_ops.rdma_cq_open(ep_ctx, 0, RDMA_COMP_SREAD);
    if (ret) {
        RDMA_PRINTERR("rdma_cq_open", ret);
        return ret;
    }

    if (!ep_ctx->av && (rdma_ctx->info->ep_attr->type == FI_EP_RDM ||
                        rdma_ctx->info->ep_attr->type == FI_EP_DGRAM)) {
        if (rdma_ctx->info->domain_attr->av_type != FI_AV_UNSPEC)
            av_attr.type = rdma_ctx->info->domain_attr->av_type;

        av_attr.count = av_size;
        ret = fi_av_open(rdma_ctx->domain, &av_attr, &ep_ctx->av, NULL);
        if (ret) {
            RDMA_PRINTERR("fi_av_open", ret);
            return ret;
        }
    }
    return 0;
}

int ep_reg_mr(ep_ctx_t *ep_ctx, void *data_buf, size_t data_buf_size)
{
    int ret;

    /* TODO: I'm using address of ep_ctx as a key,
     * maybe there is more elegant solution */
    ret = libfabric_mr_ops.rdma_reg_mr(
        ep_ctx->rdma_ctx, ep_ctx->ep, data_buf, data_buf_size,
        libfabric_mr_ops.rdma_info_to_mr_access(ep_ctx->rdma_ctx->info), (uint64_t)ep_ctx,
        FI_HMEM_SYSTEM, 0, &ep_ctx->data_mr, &ep_ctx->data_desc);
    return ret;
}

int ep_send_buf(ep_ctx_t* ep_ctx, void* buf, size_t buf_size) {
    if (!ep_ctx || !buf || buf_size == 0) {
        ERROR("Invalid parameters provided to ep_send_buf: ep_ctx=%p, buf=%p, buf_size=%zu",
              ep_ctx, buf, buf_size);
        return -EINVAL;
    }

    int ret;

    do {
         // Check if the stop flag is set
        if (ep_ctx->stop_flag) {
            ERROR("RDMA stop flag is set. Aborting send.");
            return -ECANCELED;
        }

        // Pass the buffer address as the context to fi_send
        ret = fi_send(ep_ctx->ep, buf, buf_size, ep_ctx->data_desc, ep_ctx->dest_av_entry, buf);
        if (ret == -EAGAIN) {
            struct fi_cq_entry cq_entry;
            (void)fi_cq_read(ep_ctx->cq_ctx.cq, &cq_entry, 1); // Drain CQ
        }
    } while (ret == -EAGAIN);

    return ret;
}

int ep_recv_buf(ep_ctx_t *ep_ctx, void *buf, size_t buf_size, void *buf_ctx)
{
    int ret;

    do {
         // Check if the stop flag is set
        if (ep_ctx->stop_flag) {
            ERROR("RDMA stop flag is set. Aborting receive.");
            return -ECANCELED;
        }

        ret = fi_recv(ep_ctx->ep, buf, buf_size, ep_ctx->data_desc, FI_ADDR_UNSPEC, buf_ctx);
        if (ret == -FI_EAGAIN)
            (void)fi_cq_read(ep_ctx->cq_ctx.cq, NULL, 0);
    } while (ret == -FI_EAGAIN);

    return ret;
}

int ep_cq_read(ep_ctx_t *ep_ctx, void **buf_ctx, int timeout)
{
    struct fi_cq_err_entry entry;
    int err;

    err = libfabric_cq_ops.rdma_read_cq(ep_ctx, &entry, timeout);
    if (err)
        return err;

    if (buf_ctx)
        *buf_ctx = entry.op_context;

    return 0;
}

int ep_init(ep_ctx_t **ep_ctx, ep_cfg_t *cfg)
{
    int ret;
    struct fi_info *fi;
    struct fi_info *hints;

    if (!ep_ctx || !cfg)
        return -EINVAL;

    if (cfg->rdma_ctx == NULL)
        return -EINVAL;

    *ep_ctx = calloc(1, sizeof(ep_ctx_t));
    if (!(*ep_ctx)) {
        RDMA_PRINTERR("libfabric ep context malloc fail\n", -ENOMEM);
        return -ENOMEM;
    }
    (*ep_ctx)->rdma_ctx = cfg->rdma_ctx;
    (*ep_ctx)->stop_flag = false;

    hints = fi_dupinfo((*ep_ctx)->rdma_ctx->info);
    if (!hints) {
        RDMA_PRINTERR("fi_dupinfo failed\n", -ENOMEM);
        libfabric_ep_ops.ep_destroy(ep_ctx);
        return -ENOMEM;
    }

    hints->src_addr = NULL;
    hints->src_addrlen = 0;
    hints->dest_addr = NULL;
    hints->dest_addrlen = 0;
    hints->addr_format = FI_SOCKADDR_IN;

    if (cfg->dir == RX) {
        ret = fi_getinfo(FI_VERSION(1, 21), cfg->local_addr.ip, cfg->local_addr.port, FI_SOURCE, hints, &fi);
    } else {
        ret = fi_getinfo(FI_VERSION(1, 21), cfg->remote_addr.ip, cfg->remote_addr.port, 0, hints,
                         &fi);
    }
    if (ret) {
        RDMA_PRINTERR("fi_getinfo", ret);
        libfabric_ep_ops.ep_destroy(ep_ctx);
        return ret;
    }

    ret = ep_alloc_res(*ep_ctx, (*ep_ctx)->rdma_ctx, fi, 1);
    if (ret) {
        RDMA_PRINTERR("ep_alloc_res fail\n", ret);
        libfabric_ep_ops.ep_destroy(ep_ctx);
        return ret;
    }

    ret = enable_ep((*ep_ctx));
    if (ret) {
        RDMA_PRINTERR("ep_enable fail\n", ret);
        libfabric_ep_ops.ep_destroy(ep_ctx);
        return ret;
    }

    if (fi->dest_addr) {
        ret = ep_av_insert((*ep_ctx)->rdma_ctx, (*ep_ctx)->av, fi->dest_addr, 1,
                           &(*ep_ctx)->dest_av_entry, 0, NULL);
        if (ret) {
            RDMA_PRINTERR("ep_av_insert fail\n", ret);
            libfabric_ep_ops.ep_destroy(ep_ctx);
            return ret;
        }
    }

    fi_freeinfo(fi);

    return 0;
}

int ep_destroy(ep_ctx_t **ep_ctx)
{

    if (!ep_ctx || !(*ep_ctx))
        return -EINVAL;

    libfabric_mr_ops.rdma_unreg_mr((*ep_ctx)->data_mr);
    RDMA_CLOSE_FID((*ep_ctx)->ep);

    RDMA_CLOSE_FID((*ep_ctx)->cq_ctx.cq);
    RDMA_CLOSE_FID((*ep_ctx)->av);
    RDMA_CLOSE_FID((*ep_ctx)->cq_ctx.waitset);

    if (ep_ctx && *ep_ctx) {
        free(*ep_ctx);
        *ep_ctx = NULL;
    }

    return 0;
}

struct libfabric_ep_ops_t libfabric_ep_ops = {
    .ep_reg_mr = ep_reg_mr,
    .ep_send_buf = ep_send_buf,
    .ep_recv_buf = ep_recv_buf,
    .ep_cq_read = ep_cq_read,
    .ep_init = ep_init,
    .ep_destroy = ep_destroy
};