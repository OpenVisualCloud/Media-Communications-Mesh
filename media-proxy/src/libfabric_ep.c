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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>

#include <rdma/rdma_verbs.h>
#include <rdma/rdma_cma.h>
#include <errno.h>

int get_interface_name_by_ip(const char *ip_str, char *if_name, size_t if_name_len) {
    struct ifaddrs *ifaddr, *ifa;
    int family;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    // Iterate over the linked list of interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            // Convert the interface address to a string
            if (getnameinfo(ifa->ifa_addr,
                            (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                                  sizeof(struct sockaddr_in6),
                            host, sizeof(host),
                            NULL, 0, NI_NUMERICHOST) == 0) {
                // Compare with our target IP string
                if (strcmp(host, ip_str) == 0) {
                    // Found a match, copy interface name
                    strncpy(if_name, ifa->ifa_name, if_name_len);
                    if_name[if_name_len - 1] = '\0';
                    freeifaddrs(ifaddr);
                    return 0; // Success
                }
            }
        }
    }

    freeifaddrs(ifaddr);
    return -1; // Not found
}

static int enable_ep(ep_ctx_t *ep_ctx)
{
    int ret;

    // Only bind to AV if it exists
    if (ep_ctx->av) {
        ret = fi_ep_bind(ep_ctx->ep, &ep_ctx->av->fid, 0);
        if (ret) {
            RDMA_PRINTERR("fi_ep_bind (AV)", ret);
            return ret;
        }
    } else {
        // For endpoints that don't require AV (like FI_EP_MSG)
        printf("[enable_ep] Note: No AV to bind (normal for some endpoint types)\n");
    }

    // CQ binding is required for all endpoint types
    ret = fi_ep_bind(ep_ctx->ep, &ep_ctx->cq_ctx.cq->fid, FI_SEND | FI_RECV);
    if (ret) {
        RDMA_PRINTERR("fi_ep_bind (CQ)", ret);
        return ret;
    }

    // Now enable the endpoint
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

int ep_send_buf(ep_ctx_t *ep_ctx, void *buf, size_t buf_size)
{
    if (!ep_ctx || !buf || buf_size == 0)
        return -EINVAL;

    if (ep_ctx->dest_av_entry == FI_ADDR_UNSPEC) {
        fprintf(stderr, "[ep_send_buf] ERROR: Invalid destination address\n");
        return -EINVAL;
    }

    const int max_retries   = 30;
    const int backoff_us[5] = {  50, 100, 250, 500, 1000 };   /* capped */

    void *desc = ep_ctx->data_desc ? ep_ctx->data_desc : NULL;
    int   retry = 0;
    int   ret;

    do {
        if (ep_ctx->stop_flag)
            return -ECANCELED;

        ret = fi_send(ep_ctx->ep, buf, buf_size, desc,
                      ep_ctx->dest_av_entry, buf);
        if (ret == -FI_EAGAIN || ret == -FI_ENODATA) {

            (void)fi_cq_read(ep_ctx->cq_ctx.cq, NULL, 0);

            if (retry == 0 || retry % 5 == 0)
                fprintf(stderr,
                        "[ep_send_buf] Connection not ready, retry %d/%d\n",
                        retry, max_retries);

            if (++retry > max_retries)
                return -ETIMEDOUT;

            /* short exponential back-off without polling the CQ */
            int idx = retry < 5 ? retry : 4;
            usleep(backoff_us[idx]);
        }
    } while (ret == -FI_EAGAIN || ret == -FI_ENODATA);

    if (ret)
        fprintf(stderr, "[ep_send_buf] fi_send failed: %s\n", fi_strerror(-ret));

    return ret;      /* 0 on success; <0 on error */
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

static void print_libfabric_error(const char *msg, int ret)
{
    fprintf(stderr, "%s: ret=%d (%s)\n", msg, ret, fi_strerror(-ret));
}

int ep_init(ep_ctx_t **ep_ctx, ep_cfg_t *cfg)
{
    int ret;
    struct fi_info     *old_info    = NULL;

    if (!ep_ctx || !cfg) {
        fprintf(stderr, "[ep_init] ERROR: ep_ctx/cfg is NULL\n");
        return -EINVAL;
    }

    if (!cfg->rdma_ctx) {
        fprintf(stderr, "[ep_init] ERROR: cfg->rdma_ctx is NULL\n");
        return -EINVAL;
    }

    // Validate device context is properly initialized
    if (!cfg->rdma_ctx->is_initialized || !cfg->rdma_ctx->fabric || !cfg->rdma_ctx->domain ||
        !cfg->rdma_ctx->info) {
        fprintf(stderr, "[ep_init] ERROR: Device context not properly initialized\n");
        return -EINVAL;
    }

    // Allocate endpoint context
    *ep_ctx = calloc(1, sizeof(ep_ctx_t));
    if (!(*ep_ctx)) {
        fprintf(stderr, "[ep_init] ERROR: Failed to allocate endpoint context\n");
        return -ENOMEM;
    }

    // Store reference to device context
    (*ep_ctx)->rdma_ctx = cfg->rdma_ctx;
    (*ep_ctx)->stop_flag = false;

    // Create the endpoint using the domain from the device context
    ret = fi_endpoint(cfg->rdma_ctx->domain, cfg->rdma_ctx->info, &(*ep_ctx)->ep, NULL);
    if (ret) {
        fprintf(stderr, "[ep_init] ERROR: fi_endpoint returned %d\n", ret);
        goto err;
    }

    // Initialize completion queue but use caller-supplied CQ if present
    if (cfg->shared_rx_cq) {
        (*ep_ctx)->cq_ctx.cq = cfg->shared_rx_cq;
        (*ep_ctx)->cq_ctx.external = true; // Indicate this CQ is owned by the caller
        /* NOTE: do NOT close in ep_destroy – caller owns it */
    } else {
        ret = libfabric_cq_ops.rdma_cq_open(*ep_ctx, 0, RDMA_COMP_SREAD);
        if (ret) {
            fprintf(stderr, "[ep_init] ERROR: rdma_cq_open returned %d\n", ret);
            goto err_ep;
        }
        (*ep_ctx)->cq_ctx.external = false;
        cfg->shared_rx_cq = (*ep_ctx)->cq_ctx.cq; // Store for caller
    }

    // Create address vector if needed based on endpoint type
    if (cfg->rdma_ctx->ep_attr_type == FI_EP_RDM || 
        cfg->rdma_ctx->ep_attr_type == FI_EP_DGRAM) {
        
        struct fi_av_attr av_attr = {
            .type = FI_AV_MAP,
            .count = 1
        };

        ret = fi_av_open(cfg->rdma_ctx->domain, &av_attr, &(*ep_ctx)->av, NULL);
        if (ret) {
            fprintf(stderr, "[ep_init] ERROR: fi_av_open returned %d\n", ret);
            goto err_cq;
        }
    }

    // Enable the endpoint
    ret = enable_ep(*ep_ctx);
    if (ret) {
        fprintf(stderr, "[ep_init] ERROR: enable_ep returned %d\n", ret);
        goto err_av;
    }

    // Insert destination address if available (TX mode)
    if (cfg->dir == TX && cfg->rdma_ctx->info->dest_addr) {
        ret = ep_av_insert((*ep_ctx)->rdma_ctx, (*ep_ctx)->av, 
                          cfg->rdma_ctx->info->dest_addr, 1,
                          &(*ep_ctx)->dest_av_entry, 0, NULL);
        if (ret) {
            fprintf(stderr, "[ep_init] ERROR: ep_av_insert returned %d\n", ret);
            goto err_av;
        }
    }

    fprintf(stderr, "[ep_init] Endpoint successfully initialized\n");
    return 0;

err_av:
    if ((*ep_ctx)->av) {
        fi_close(&(*ep_ctx)->av->fid);
    }

err_cq:
    if ((*ep_ctx)->cq_ctx.cq) {
        fi_close(&(*ep_ctx)->cq_ctx.cq->fid);
    }
    if ((*ep_ctx)->cq_ctx.waitset) {
        fi_close(&(*ep_ctx)->cq_ctx.waitset->fid);
    }

err_ep:
    if ((*ep_ctx)->ep) {
        fi_close(&(*ep_ctx)->ep->fid);
    }

err:
    free(*ep_ctx);
    *ep_ctx = NULL;
    return ret;
}

int ep_destroy(ep_ctx_t **ep_ctx)
{
    if (!ep_ctx || !(*ep_ctx))
        return -EINVAL;

    ep_ctx_t *ctx = *ep_ctx;

    /* 1. unregister MR (ignore ENOENT-like errors) */
    if (ctx->data_mr)
        libfabric_mr_ops.rdma_unreg_mr(ctx->data_mr);

    /* 2. Close endpoint object itself */
    RDMA_CLOSE_FID(ctx->ep);

    /* 3. Close CQ only if this EP owns it
     *    (ctx->cq_ctx.external == true  ⇒ shared CQ owned by caller) */
    if (ctx->cq_ctx.cq && !ctx->cq_ctx.external)
        RDMA_CLOSE_FID(ctx->cq_ctx.cq);

    /* 3a. The waitset belongs to the CQ; close only if we closed the CQ */
    if (ctx->cq_ctx.waitset && !ctx->cq_ctx.external)
        RDMA_CLOSE_FID(ctx->cq_ctx.waitset);

    /* 4. Address vector always owned by the EP */
    RDMA_CLOSE_FID(ctx->av);

    /* 5. finally free the context struct */
    free(ctx);
    *ep_ctx = NULL;

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