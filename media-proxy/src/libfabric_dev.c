/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <rdma/fi_cm.h>

#include "libfabric_dev.h"

/* We need to free any data that we allocated before freeing the
 * hints.
 */
static void rdma_freehints(struct fi_info *hints)
{
    if (!hints)
        return;

    if (hints->domain_attr->name) {
        free(hints->domain_attr->name);
        hints->domain_attr->name = NULL;
    }
    if (hints->fabric_attr->name) {
        free(hints->fabric_attr->name);
        hints->fabric_attr->name = NULL;
    }
    if (hints->fabric_attr->prov_name) {
        free(hints->fabric_attr->prov_name);
        hints->fabric_attr->prov_name = NULL;
    }
    if (hints->src_addr) {
        free(hints->src_addr);
        hints->src_addr = NULL;
        hints->src_addrlen = 0;
    }
    if (hints->dest_addr) {
        free(hints->dest_addr);
        hints->dest_addr = NULL;
        hints->dest_addrlen = 0;
    }

    fi_freeinfo(hints);
}

static int rdma_init_fabric(libfabric_ctx *rdma_ctx, struct fi_info *hints)
{
    int ret;

    ret = fi_getinfo(FI_VERSION(1, 21), NULL, NULL, 0, hints, &rdma_ctx->info);
    if (ret) {
        RDMA_PRINTERR("fi_getinfo", ret);
        return ret;
    }

    ret = fi_fabric(rdma_ctx->info->fabric_attr, &rdma_ctx->fabric, NULL);
    if (ret) {
        RDMA_PRINTERR("fi_fabric", ret);
        return ret;
    }

    /* TODO: future improvement: Add and monitor EQ to catch errors.
     * ret = fi_eq_open(rdma_ctx->fabric, &eq_attr, &eq, NULL);
     * if (ret) {
     *     RDMA_PRINTERR("fi_eq_open", ret);
     *     return ret;
     * } */

    ret = fi_domain(rdma_ctx->fabric, rdma_ctx->info, &rdma_ctx->domain, NULL);
    if (ret) {
        RDMA_PRINTERR("fi_domain", ret);
        return ret;
    }

    /* TODO: future improvement: Add and monitor EQ to catch errors.
     * ret = fi_domain_bind(rdma_ctx->domain, &eq->fid, 0);
     * if (ret) {
     *     RDMA_PRINTERR("fi_domain_bind", ret);
     *     return ret;
     * } */

    return 0;
}

// int rdma_init(libfabric_ctx **ctx)
// {
//     struct fi_info *hints;
//     int op, ret = 0;
//     *ctx = calloc(1, sizeof(libfabric_ctx));
//     if (*ctx == NULL) {
//         RDMA_PRINTERR("calloc", -ENOMEM);
//         return -ENOMEM;
//     }

//     hints = fi_allocinfo();
//     if (!hints) {
//         RDMA_PRINTERR("fi_allocinfo", ret);
//         libfabric_dev_ops.rdma_deinit(ctx);
//         return -ENOMEM;
//     }

//     hints->fabric_attr->prov_name = strdup("verbs");

//     hints->caps = FI_MSG;
//     hints->domain_attr->resource_mgmt = FI_RM_ENABLED; /* TODO: check performance */
//     hints->mode = FI_CONTEXT;
//     hints->domain_attr->threading = FI_THREAD_SAFE; /* TODO: check performance */
//     hints->addr_format = FI_FORMAT_UNSPEC;
//     hints->ep_attr->type = FI_EP_RDM;
//     hints->domain_attr->mr_mode =
//         FI_MR_LOCAL | FI_MR_ENDPOINT | (FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR);
//     hints->tx_attr->tclass = FI_TC_BULK_DATA;

//     ret = rdma_init_fabric(*ctx, hints);
//     rdma_freehints(hints);
//     if (ret) {
//         libfabric_dev_ops.rdma_deinit(ctx);
//         return ret;
//     }

//     return 0;
// }

int rdma_init(libfabric_ctx **ctx)
{
    struct fi_info *hints;
    int op, ret = 0;
    *ctx = calloc(1, sizeof(libfabric_ctx));
    if (*ctx == NULL) {
        RDMA_PRINTERR("calloc", -ENOMEM);
        return -ENOMEM;
    }

    hints = fi_allocinfo();
    if (!hints) {
        RDMA_PRINTERR("fi_allocinfo", ret);
        libfabric_dev_ops.rdma_deinit(ctx);
        return -ENOMEM;
    }

    hints->fabric_attr->prov_name = strdup("verbs");
    //hints->fabric_attr->prov_name = strdup("ofi_rxm");
    //hints->caps = FI_MSG;
    hints->caps = FI_MSG | FI_RMA | FI_WRITE | FI_READ | FI_REMOTE_READ | FI_REMOTE_WRITE;
    hints->domain_attr->resource_mgmt = FI_RM_DISABLED; 
    //hints->domain_attr->resource_mgmt = FI_RM_ENABLED; /* TODO: check performance */
    hints->mode = FI_CONTEXT;
    //hints->domain_attr->threading = FI_THREAD_SAFE; /* TODO: check performance */
    hints->domain_attr->threading = FI_THREAD_UNSPEC;
    //hints->addr_format = FI_FORMAT_UNSPEC;
    hints->addr_format = FI_SOCKADDR_IN;
    //hints->ep_attr->type = FI_EP_RDM;
    hints->ep_attr->type = FI_EP_RDM | FI_EP_MSG | FI_EP_RDM | FI_EP_DGRAM;
    //hints->domain_attr->mr_mode = FI_MR_LOCAL | FI_MR_ENDPOINT | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR; 
     hints->domain_attr->mr_mode =
         FI_MR_LOCAL | FI_MR_ENDPOINT | (FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR);
    hints->tx_attr->tclass = FI_TC_BULK_DATA;
    
    ret = rdma_init_fabric(*ctx, hints);
    rdma_freehints(hints);
    if (ret) {
        libfabric_dev_ops.rdma_deinit(ctx);
        return ret;
    }

    return 0;
}

static void rdma_free_res(libfabric_ctx *rdma_ctx)
{
    int ret;

    RDMA_CLOSE_FID(rdma_ctx->domain);
    RDMA_CLOSE_FID(rdma_ctx->fabric);

    if (rdma_ctx->info) {
        fi_freeinfo(rdma_ctx->info);
        rdma_ctx->info = NULL;
    }
}

int rdma_deinit(libfabric_ctx **ctx)
{

    if (!ctx || !(*ctx))
        return -EINVAL;

    rdma_free_res(*ctx);

    if ((*ctx)->info) {
        fi_freeinfo((*ctx)->info);
        (*ctx)->info = NULL;
    }

    if (ctx && *ctx) {
        free(*ctx);
        *ctx = NULL;
    }

    return 0;
}

libfabric_dev_ops_t libfabric_dev_ops = {
    .rdma_init = rdma_init,
    .rdma_deinit = rdma_deinit,
};
