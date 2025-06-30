/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <rdma/fi_cm.h>
#include <rdma/rdma_verbs.h>
#include <rdma/rdma_cma.h>

#include "libfabric_dev.h"

const char *LIB_FABRIC_ATTR_PROV_NAME_TCP = "tcp";
const char *LIB_FABRIC_ATTR_PROV_NAME_VERBS = "verbs";

/* We need to free any data that we allocated before freeing the
 * hints.
 */
static void rdma_freehints(struct fi_info *hints)
{
    if (!hints)
        return;

    if (hints->tx_attr) {
        free(hints->tx_attr);
        hints->tx_attr = NULL;
    }
    if (hints->rx_attr) {
        free(hints->rx_attr);
        hints->rx_attr = NULL;
    }

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

    ret = fi_getinfo(FI_VERSION(2, 0), NULL, NULL, 0, hints, &rdma_ctx->info);
    if (ret) {
        RDMA_PRINTERR("fi_getinfo", ret);
        return ret;
    }

    ret = fi_fabric(rdma_ctx->info->fabric_attr, &rdma_ctx->fabric, NULL);
    if (ret) {
        RDMA_PRINTERR("fi_fabric", ret);
        return ret;
    }

    ret = fi_domain(rdma_ctx->fabric, rdma_ctx->info, &rdma_ctx->domain, NULL);
    if (ret) {
        RDMA_PRINTERR("fi_domain", ret);
        return ret;
    }

    /* TODO: future improvement: Add and monitor EQ to catch errors.
     * ret = fi_eq_open(rdma_ctx->fabric, &eq_attr, &eq, NULL);
     * if (ret) {
     *     RDMA_PRINTERR("fi_eq_open", ret);
     *     return ret;
     * } */

    /* TODO: future improvement: Add and monitor EQ to catch errors.
     * ret = fi_domain_bind(rdma_ctx->domain, &eq->fid, 0);
     * if (ret) {
     *     RDMA_PRINTERR("fi_domain_bind", ret);
     *     return ret;
     * } */

    return 0;
}

int rdma_init(libfabric_ctx **ctx)
{
    printf("[rdma_init] Entering function\n");
    struct fi_info *hints = NULL;
    struct rdma_addrinfo *rai_res = NULL;
    struct rdma_addrinfo rai_hints;
    int ret = 0;

    if (!ctx || !(*ctx)) {
        printf("[rdma_init] ERROR: ctx is NULL\n");
        return -EINVAL;
    }

    // Clear any previous initialization
    if ((*ctx)->fabric || (*ctx)->domain || (*ctx)->info) {
        printf("[rdma_init] WARNING: Context already contains initialized resources\n");
        rdma_free_res(*ctx);
    }

    memset(&rai_hints, 0, sizeof(rai_hints));
    printf("[rdma_init] Initialized rai_hints to zero\n");

    /* Allocate fi_info hints */
    hints = fi_allocinfo();
    if (!hints) {
        printf("[rdma_init] ERROR: fi_allocinfo failed\n");
        return -ENOMEM;
    }
    printf("[rdma_init] fi_allocinfo returned valid hints at %p\n", hints);

    if ((*ctx)->provider_name && strcmp((*ctx)->provider_name, "verbs") == 0) {
        hints->fabric_attr->prov_name = strdup(LIB_FABRIC_ATTR_PROV_NAME_VERBS);
        hints->ep_attr->type          = FI_EP_RDM;  // Reliable datagram
        hints->ep_attr->protocol      = FI_PROTO_RXM;
        hints->addr_format            = FI_SOCKADDR_IN;  // IPv4
        hints->domain_attr->av_type   = FI_AV_UNSPEC;
        hints->domain_attr->mr_mode   =
            FI_MR_LOCAL | FI_MR_ALLOCATED | FI_MR_VIRT_ADDR | FI_MR_PROV_KEY;
        hints->domain_attr->data_progress = FI_PROGRESS_AUTO; // Use auto progress

        /* Adjust capabilities based on the direction */
        if ((*ctx)->kind == FI_KIND_RECEIVER) {
            hints->caps = FI_MSG | FI_RECV | FI_RMA | FI_REMOTE_READ | FI_LOCAL_COMM;
            hints->rx_attr = calloc(1, sizeof(struct fi_rx_attr));
            if (hints->rx_attr) {
                hints->rx_attr->size = 1024;  // Larger receive queue
            }
        } else {
            hints->caps = FI_MSG | FI_SEND | FI_RMA | FI_REMOTE_WRITE | FI_LOCAL_COMM;
            hints->tx_attr = calloc(1, sizeof(struct fi_tx_attr));
            if (hints->tx_attr) {
                hints->tx_attr->size = 1024;  // Larger send queue
            }
        }
    } else {
        hints->domain_attr->mr_mode =
            FI_MR_LOCAL | FI_MR_VIRT_ADDR;
        hints->ep_attr->type = FI_EP_RDM;
        hints->caps = FI_MSG;
        hints->addr_format = FI_SOCKADDR_IN;
        hints->fabric_attr->prov_name = strdup(LIB_FABRIC_ATTR_PROV_NAME_TCP);
        hints->tx_attr->tclass = FI_TC_BULK_DATA;
        hints->domain_attr->resource_mgmt = FI_RM_ENABLED;
        hints->mode = FI_OPT_ENDPOINT;
        hints->tx_attr->size = 1024; // Transmit queue size
        hints->rx_attr->size = 1024; // Receive queue size
        hints->domain_attr->threading = FI_THREAD_UNSPEC;
    }

    /* Store configuration for later endpoint use */
    (*ctx)->is_initialized = false;
    (*ctx)->ep_attr_type = hints->ep_attr->type;
    (*ctx)->addr_format = hints->addr_format;

    /* Adjust capabilities based on the connection kind */
    if ((*ctx)->kind == FI_KIND_RECEIVER) {
        /* For RX, resolve the local address */
        ret = rdma_getaddrinfo((*ctx)->local_ip, (*ctx)->local_port, &rai_hints, &rai_res);
        if (ret) {
            printf("[rdma_init] ERROR: rdma_getaddrinfo (RX) returned %d\n", ret);
            goto err;
        }

        hints->src_addr = malloc(rai_res->ai_src_len);
        if (!hints->src_addr) {
            ret = -ENOMEM;
            goto err;
        }

        // Set the FI_SOURCE flag to tell provider to use src_addr for binding
        hints->caps |= FI_SOURCE;

        memcpy(hints->src_addr, rai_res->ai_src_addr, rai_res->ai_src_len);
        hints->src_addrlen = rai_res->ai_src_len;
    } else {
        /* For TX, resolve the remote address */
        ret = rdma_getaddrinfo((*ctx)->remote_ip, (*ctx)->remote_port, &rai_hints, &rai_res);
        if (ret) {
            printf("[rdma_init] ERROR: rdma_getaddrinfo (TX) returned %d\n", ret);
            goto err;
        }

        // Handle source address
        if (rai_res->ai_src_len > 0) {
            hints->src_addr = malloc(rai_res->ai_src_len);
            if (!hints->src_addr) {
                ret = -ENOMEM;
                goto err;
            }
            memcpy(hints->src_addr, rai_res->ai_src_addr, rai_res->ai_src_len);
            hints->src_addrlen = rai_res->ai_src_len;
        }

        // Handle destination address
        if (rai_res->ai_dst_len > 0) {
            hints->dest_addr = malloc(rai_res->ai_dst_len);
            if (!hints->dest_addr) {
                ret = -ENOMEM;
                goto err;
            }
            memcpy(hints->dest_addr, rai_res->ai_dst_addr, rai_res->ai_dst_len);
            hints->dest_addrlen = rai_res->ai_dst_len;
        }
    }

    /* Create fabric info, domain and fabric */
    // For RX, pass hostname and port to enforce binding
    if ((*ctx)->kind == FI_KIND_RECEIVER) {
        ret = fi_getinfo(FI_VERSION(2, 0), (*ctx)->local_ip, (*ctx)->local_port, 
                        FI_SOURCE, hints, &(*ctx)->info);
    } else {
        // For TX, use the remote address info to force connectivity to specific endpoint
        printf("[rdma_init] TX - Enforcing connection to remote %s:%s\n", (*ctx)->remote_ip,
               (*ctx)->remote_port);

        ret = fi_getinfo(FI_VERSION(2, 0), (*ctx)->remote_ip, (*ctx)->remote_port, 0, hints,
                         &(*ctx)->info);
    }
    if (ret) {
        fprintf(stderr, "[rdma_init] ERROR: fi_getinfo returned %d (%s)\n", ret, fi_strerror(-ret));
        goto err;
    }

    ret = fi_fabric((*ctx)->info->fabric_attr, &(*ctx)->fabric, NULL);
    if (ret) {
        printf("[rdma_init] ERROR: fi_fabric returned %d\n", ret);
        goto err_info;
    }

    ret = fi_domain((*ctx)->fabric, (*ctx)->info, &(*ctx)->domain, NULL);
    if (ret) {
        printf("[rdma_init] ERROR: fi_domain returned %d\n", ret);
        goto err_fabric;
    }

    /* Clean up temporary resources */
    rdma_freeaddrinfo(rai_res);
    rdma_freehints(hints);  // We've already stored the info in ctx->info

    (*ctx)->is_initialized = true;
    printf("[rdma_init] Device context successfully initialized\n");
    return 0;

err_fabric:
    fi_close(&(*ctx)->fabric->fid);
    
err_info:
    fi_freeinfo((*ctx)->info);
    free(*ctx);
    *ctx = NULL;
    
err:
    rdma_freeaddrinfo(rai_res);
    rdma_freehints(hints);
    fprintf(stderr, "[rdma_init] Failed with error %d\n", ret);
    return ret;
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
