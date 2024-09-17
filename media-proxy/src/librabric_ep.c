#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

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
    uint64_t flags;
    int ret;

    RDMA_EP_BIND(ep_ctx->ep, ep_ctx->av, 0);

    flags = FI_TRANSMIT;
    RDMA_EP_BIND(ep_ctx->ep, ep_ctx->txcq, flags);

    flags = FI_RECV;
    RDMA_EP_BIND(ep_ctx->ep, ep_ctx->rxcq, flags);

    ret = rdma_get_cq_fd(ep_ctx->txcq, &ep_ctx->tx_fd, ep_ctx->rdma_ctx->comp_method);
    if (ret)
        return ret;

    ret = rdma_get_cq_fd(ep_ctx->rxcq, &ep_ctx->rx_fd, ep_ctx->rdma_ctx->comp_method);
    if (ret)
        return ret;

    ret = fi_enable(ep_ctx->ep);
    if (ret) {
        RDMA_PRINTERR("fi_enable", ret);
        return ret;
    }

    return 0;
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
        return -EXIT_FAILURE;
    }

    return 0;
}

int ep_init_av_addr(ep_ctx_t *ep_ctx, libfabric_ctx *rdma_ctx, struct fi_info *fi)
{
    size_t addrlen;
    int ret;

    if (fi->dest_addr) {
        ret = ep_av_insert(rdma_ctx, ep_ctx->av, fi->dest_addr, 1, &ep_ctx->dest_av_entry, 0, NULL);
        if (ret)
            return ret;

        addrlen = ep_ctx->data_buf_size;
        ret = fi_getname(&ep_ctx->ep->fid, (char *)ep_ctx->data_buf, &addrlen);
        if (ret) {
            RDMA_PRINTERR("fi_getname", ret);
            return ret;
        }
        struct sockaddr_in *insert_addr_p = (struct sockaddr_in *)ep_ctx->data_buf;
        memset(insert_addr_p->sin_zero, 0, sizeof(insert_addr_p->sin_zero));

        ret = ep_send_buf(ep_ctx, ep_ctx->data_buf, addrlen);
        if (ret)
            return ret;

    } else {
        ret = ep_recv_buf(ep_ctx, ep_ctx->data_buf, ep_ctx->data_buf_size);
        if (ret)
            return ret;

        ret = ep_av_insert(rdma_ctx, ep_ctx->av, ep_ctx->data_buf, 1, &ep_ctx->dest_av_entry, 0,
                           NULL);
        if (ret)
            return ret;
    }

    return 0;
}

static int ep_alloc_res(ep_ctx_t *ep_ctx, libfabric_ctx *rdma_ctx, struct fi_info *fi,
                        size_t tx_cq_size, size_t rx_cq_size, size_t av_size)
{
    int ret;

    ret = fi_endpoint(rdma_ctx->domain, fi, &ep_ctx->ep, NULL);
    if (ret) {
        RDMA_PRINTERR("fi_endpoint", ret);
        return ret;
    }

    struct fi_cq_attr cq_attr = {.wait_obj = FI_WAIT_NONE};

    if (cq_attr.format == FI_CQ_FORMAT_UNSPEC) {
        cq_attr.format = FI_CQ_FORMAT_CONTEXT;
    }

    rdma_cq_set_wait_attr(cq_attr, rdma_ctx->comp_method, NULL);
    if (tx_cq_size)
        cq_attr.size = tx_cq_size;
    else
        cq_attr.size = rdma_ctx->info->tx_attr->size;

    ret = fi_cq_open(rdma_ctx->domain, &cq_attr, &ep_ctx->txcq, ep_ctx);
    if (ret) {
        RDMA_PRINTERR("fi_cq_open", ret);
        return ret;
    }

    rdma_cq_set_wait_attr(cq_attr, rdma_ctx->comp_method, NULL);
    if (rx_cq_size)
        cq_attr.size = rx_cq_size;
    else
        cq_attr.size = rdma_ctx->info->rx_attr->size;

    ret = fi_cq_open(rdma_ctx->domain, &cq_attr, &ep_ctx->rxcq, ep_ctx);
    if (ret) {
        RDMA_PRINTERR("fi_cq_open", ret);
        return ret;
    }

    struct fi_av_attr av_attr = {.type = FI_AV_MAP, .count = 1};

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

static int ep_reg_mr(ep_ctx_t *ep_ctx, libfabric_ctx *rdma_ctx, struct fi_info *fi)
{
    int ret;

    // TODO: I'm using address of ep_ctx as a key, maybe there is more elegant solution
    ret = rdma_reg_mr(rdma_ctx, ep_ctx->ep, fi, ep_ctx->data_buf, ep_ctx->data_buf_size,
                      rdma_info_to_mr_access(fi), (uint64_t)ep_ctx, FI_HMEM_SYSTEM, 0,
                      &ep_ctx->data_mr, &ep_ctx->data_desc);
    if (ret)
        return ret;

    return 0;
}

int ep_send_buf(ep_ctx_t *ep_ctx, char *buf, size_t buf_size)
{
    int ret;

    do {
        ret = fi_send(ep_ctx->ep, buf, buf_size, ep_ctx->data_desc, ep_ctx->dest_av_entry,
                      ep_ctx->send_ctx);
        if (ret == -FI_EAGAIN)
            (void)fi_cq_read(ep_ctx->txcq, NULL, 0);
    } while (ret == -FI_EAGAIN);

    ret = rdma_get_cq_comp(ep_ctx, ep_ctx->txcq, &ep_ctx->tx_cq_cntr, ep_ctx->tx_cq_cntr + 1, -1);
    if (ret)
        return ret;

    return 0;
}

int ep_recv_buf(ep_ctx_t *ep_ctx, char *buf, size_t buf_size)
{
    int ret;
    double elapsed_time;
    double fps = 0.0;

    do {
        ret =
            fi_recv(ep_ctx->ep, buf, buf_size, ep_ctx->data_desc, FI_ADDR_UNSPEC, ep_ctx->recv_ctx);
        if (ret == -FI_EAGAIN)
            (void)fi_cq_read(ep_ctx->rxcq, NULL, 0);
    } while (ret == -FI_EAGAIN);

    ret = rdma_get_cq_comp(ep_ctx, ep_ctx->rxcq, &ep_ctx->rx_cq_cntr, ep_ctx->rx_cq_cntr + 1, -1);
    if (ret)
        return ret;

    return 0;
}

int ep_init(ep_ctx_t **ep_ctx, ep_cfg_t *cfg)
{
    int ret;
    struct fi_info *fi;
    struct fi_info *hints;

    *ep_ctx = calloc(1, sizeof(ep_ctx_t));
    if (!(*ep_ctx)) {
        printf("%s, libfabric ep context malloc fail\n", __func__);
        return EXIT_FAILURE;
    }
    (*ep_ctx)->rdma_ctx = cfg->rdma_ctx;
    (*ep_ctx)->data_buf = cfg->data_buf;
    (*ep_ctx)->data_buf_size = cfg->data_buf_size;

    hints = fi_dupinfo((*ep_ctx)->rdma_ctx->info);

    hints->src_addr = NULL;
    hints->src_addrlen = 0;
    hints->dest_addr = NULL;
    hints->dest_addrlen = 0;
    hints->addr_format = FI_SOCKADDR_IN;

    if (cfg->dir == RX) {
        ret = fi_getinfo(FI_VERSION(1, 21), NULL, cfg->local_addr.port, FI_SOURCE, hints, &fi);
    } else {
        ret = fi_getinfo(FI_VERSION(1, 21), cfg->remote_addr.ip, cfg->remote_addr.port, 0, hints,
                         &fi);
    }
    if (ret) {
        RDMA_PRINTERR("fi_getinfo", ret);
        return ret;
    }

    ret = ep_reg_mr(*ep_ctx, (*ep_ctx)->rdma_ctx, fi);
    if (ret) {
        printf("%s, ep_reg_mr fail\n", __func__);
        ep_destroy(ep_ctx);
        return ret;
    }

    ret = ep_alloc_res(*ep_ctx, (*ep_ctx)->rdma_ctx, fi, 0, 0, 1);
    if (ret) {
        printf("%s, ep_alloc_res fail\n", __func__);
        ep_destroy(ep_ctx);
        return ret;
    }

    ret = enable_ep((*ep_ctx));
    if (ret) {
        printf("%s, ep_enable fail\n", __func__);
        ep_destroy(ep_ctx);
        return ret;
    }

    ret = ep_init_av_addr((*ep_ctx), (*ep_ctx)->rdma_ctx, fi);
    if (ret)
        return ret;

    fi_freeinfo(fi);

    return 0;
}

int ep_destroy(ep_ctx_t **ep_ctx)
{

    if (!ep_ctx || !(*ep_ctx))
        return -EINVAL;

    // TODO: make sure that you close everything inside ep_ctx
    RDMA_CLOSE_FID((*ep_ctx)->data_mr);
    RDMA_CLOSE_FID((*ep_ctx)->ep);

    RDMA_CLOSE_FID((*ep_ctx)->txcq);
    RDMA_CLOSE_FID((*ep_ctx)->rxcq);
    RDMA_CLOSE_FID((*ep_ctx)->av);
    RDMA_CLOSE_FID((*ep_ctx)->waitset);

    if (ep_ctx && *ep_ctx) {
        free(*ep_ctx);
        *ep_ctx = NULL;
    }

    return 0;
}
