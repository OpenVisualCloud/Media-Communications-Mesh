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

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>

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
#include "mesh/logger.h"

using namespace mesh;

static int enable_ep(ep_ctx_t* ep_ctx) {
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

static int ep_av_insert(libfabric_ctx* rdma_ctx, struct fid_av* av, void* addr, size_t count,
                        fi_addr_t* fi_addr, uint64_t flags, void* context) {
    int ret;

    ret = fi_av_insert(av, addr, count, fi_addr, flags, context);
    if (ret < 0) {
        RDMA_PRINTERR("fi_av_insert", ret);
        return ret;
    } else if (ret != static_cast<int>(count)) {
        RDMA_ERR("fi_av_insert: number of addresses inserted = %d;"
                 " number of addresses given = %zu\n",
                 ret, count);
        return -EINVAL;
    }

    return 0;
}

static int ep_alloc_res(ep_ctx_t* ep_ctx, libfabric_ctx* rdma_ctx, struct fi_info* fi,
                        size_t av_size) {
    struct fi_av_attr av_attr = {.type = FI_AV_MAP, .count = 1};
    int ret;

    ret = fi_endpoint(rdma_ctx->domain, fi, &ep_ctx->ep, nullptr);
    if (ret) {
        RDMA_PRINTERR("fi_endpoint", ret);
        return ret;
    }

    rdma_cq_open(ep_ctx, 0, RDMA_COMP_SREAD);

    if (!ep_ctx->av && (rdma_ctx->info->ep_attr->type == FI_EP_RDM ||
                        rdma_ctx->info->ep_attr->type == FI_EP_DGRAM)) {
        if (rdma_ctx->info->domain_attr->av_type != FI_AV_UNSPEC)
            av_attr.type = rdma_ctx->info->domain_attr->av_type;

        av_attr.count = av_size;
        ret = fi_av_open(rdma_ctx->domain, &av_attr, &ep_ctx->av, nullptr);
        if (ret) {
            RDMA_PRINTERR("fi_av_open", ret);
            return ret;
        }
    }
    return 0;
}

int ep_reg_mr(ep_ctx_t* ep_ctx, void* data_buf, size_t data_buf_size) {
    int ret;

    /* TODO: I'm using address of ep_ctx as a key,
     * maybe there is more elegant solution */
    ret = rdma_reg_mr(ep_ctx->rdma_ctx, ep_ctx->ep, data_buf, data_buf_size,
                       rdma_info_to_mr_access(ep_ctx->rdma_ctx->info), reinterpret_cast<uint64_t>(ep_ctx),
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
    int attempts = 0;

    do {
        ret = fi_send(ep_ctx->ep, buf, buf_size, ep_ctx->data_desc, ep_ctx->dest_av_entry, nullptr);
        if (ret == -EAGAIN) {
            (void)fi_cq_read(ep_ctx->cq_ctx.cq, nullptr, 0);
            ++attempts;
        }
    } while (ret == -EAGAIN && attempts < 100000);

    return ret;
}

int ep_recv_buf(ep_ctx_t* ep_ctx, void* buf, size_t buf_size, void* buf_ctx) {
    int ret;

    do {
        ret = fi_recv(ep_ctx->ep, buf, buf_size, ep_ctx->data_desc, FI_ADDR_UNSPEC, buf_ctx);
        if (ret == -FI_EAGAIN)
            (void)fi_cq_read(ep_ctx->cq_ctx.cq, nullptr, 0);
    } while (ret == -FI_EAGAIN);

    return ret;
}

int ep_cq_read(ep_ctx_t* ep_ctx, void** buf_ctx, int timeout) {
    struct fi_cq_err_entry entry;
    int err;

    err = rdma_read_cq(ep_ctx, &entry, timeout);
    if (err)
        return err;

    if (buf_ctx)
        *buf_ctx = entry.op_context;

    return 0;
}

int ep_init(ep_ctx_t** ep_ctx, ep_cfg_t* cfg) {
    mesh::log::info("ep_init started");

    int ret;
    struct fi_info* fi = nullptr;
    struct fi_info* hints = nullptr;

    log::info("Allocating memory for ep_ctx");
    *ep_ctx = static_cast<ep_ctx_t*>(calloc(1, sizeof(ep_ctx_t)));
    if (!(*ep_ctx)) {
        log::error("Memory allocation for ep_ctx failed")("function", __func__);
        return -ENOMEM;
    }
    log::info("Memory allocation successful")("ep_ctx_address", static_cast<void*>(*ep_ctx));

    (*ep_ctx)->rdma_ctx = cfg->rdma_ctx;
    log::debug("Assigned rdma_ctx to ep_ctx")("rdma_ctx", static_cast<void*>(cfg->rdma_ctx));

    log::info("Duplicating hints from RDMA context info");
    hints = fi_dupinfo((*ep_ctx)->rdma_ctx->info);
    if (!hints) {
        log::error("fi_dupinfo failed");
        free(*ep_ctx);
        return -ENOMEM;
    }

    hints->src_addr = nullptr;
    hints->src_addrlen = 0;
    hints->dest_addr = nullptr;
    hints->dest_addrlen = 0;
    hints->addr_format = FI_SOCKADDR_IN;

    if (cfg->dir == RX) {
        log::info("Direction is RX, fetching info using fi_getinfo")
                 ("version", FI_VERSION(1, 21))
                 ("local_port", cfg->local_addr.port);
        ret = fi_getinfo(FI_VERSION(1, 21), NULL, cfg->local_addr.port, FI_SOURCE, hints, &fi);
    } else {
        log::info("Direction is TX, fetching info using fi_getinfo")
                 ("version", FI_VERSION(1, 21))
                 ("remote_ip", cfg->remote_addr.ip)
                 ("remote_port", cfg->remote_addr.port);
        ret = fi_getinfo(FI_VERSION(1, 21), cfg->remote_addr.ip, cfg->remote_addr.port, 0, hints, &fi);
    }

    if (ret) {
        log::error("fi_getinfo failed")("return_code", ret);
        RDMA_PRINTERR("fi_getinfo", ret);
        fi_freeinfo(hints);
        free(*ep_ctx);
        return ret;
    }
    log::info("fi_getinfo successful")("fi", static_cast<void*>(fi));

    log::info("Allocating RDMA endpoint resources");
    ret = ep_alloc_res(*ep_ctx, (*ep_ctx)->rdma_ctx, fi, 1);
    if (ret) {
        log::error("ep_alloc_res failed")("return_code", ret);
        ep_destroy(ep_ctx);
        fi_freeinfo(fi);
        return ret;
    }
    log::info("RDMA endpoint resources allocated successfully");

    log::info("Enabling endpoint");
    ret = enable_ep((*ep_ctx));
    if (ret) {
        log::error("enable_ep failed")("return_code", ret);
        ep_destroy(ep_ctx);
        fi_freeinfo(fi);
        return ret;
    }
    log::info("Endpoint enabled successfully");

    if (fi->dest_addr) {
        log::info("Inserting destination address into address vector")
                 ("dest_addr", static_cast<void*>(fi->dest_addr));
        ret = ep_av_insert((*ep_ctx)->rdma_ctx, (*ep_ctx)->av, fi->dest_addr, 1,
                           &(*ep_ctx)->dest_av_entry, 0, nullptr);
        if (ret) {
            log::error("ep_av_insert failed")("return_code", ret);
            fi_freeinfo(fi);
            return ret;
        }
        log::info("Destination address successfully inserted into address vector")
                 ("dest_av_entry", (*ep_ctx)->dest_av_entry);
    }

    fi_freeinfo(fi);
    log::info("ep_init completed successfully");
    return 0;
}



int ep_destroy(ep_ctx_t** ep_ctx) {
    if (!ep_ctx || !(*ep_ctx))
        return -EINVAL;

    RDMA_CLOSE_FID((*ep_ctx)->data_mr);
    RDMA_CLOSE_FID((*ep_ctx)->ep);

    RDMA_CLOSE_FID((*ep_ctx)->cq_ctx.cq);
    RDMA_CLOSE_FID((*ep_ctx)->av);
    RDMA_CLOSE_FID((*ep_ctx)->cq_ctx.waitset);

    if (ep_ctx && *ep_ctx) {
        free(*ep_ctx);
        *ep_ctx = nullptr;
    }

    return 0;
}