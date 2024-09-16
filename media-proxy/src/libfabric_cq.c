#include <assert.h>

#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_collective.h>

#include "libfabric_cq.h"

#define CQ_TIMEOUT (-1)

void rdma_cq_set_wait_attr(struct fi_cq_attr cq_attr, enum cq_comp_method method, struct fid_wait *waitset)
{
    switch (method) {
    case RDMA_COMP_SREAD:
        cq_attr.wait_obj = FI_WAIT_UNSPEC;
        cq_attr.wait_cond = FI_CQ_COND_NONE;
        break;
    case RDMA_COMP_WAITSET:
        assert(waitset);
        cq_attr.wait_obj = FI_WAIT_SET;
        cq_attr.wait_cond = FI_CQ_COND_NONE;
        cq_attr.wait_set = waitset;
        break;
    case RDMA_COMP_WAIT_FD:
        cq_attr.wait_obj = FI_WAIT_FD;
        cq_attr.wait_cond = FI_CQ_COND_NONE;
        break;
    case RDMA_COMP_YIELD:
        cq_attr.wait_obj = FI_WAIT_YIELD;
        cq_attr.wait_cond = FI_CQ_COND_NONE;
        break;
    default:
        cq_attr.wait_obj = FI_WAIT_NONE;
        break;
    }
}

/*
 * fi_cq_err_entry can be cast to any CQ entry format.
 */
static int rdma_spin_for_comp(ep_ctx_t* ep_ctx, struct fid_cq *cq, uint64_t *cur,
                uint64_t total, int timeout)
{
    struct fi_cq_err_entry comp;
    struct timespec a, b;
    int ret;

    if (timeout >= 0)
        clock_gettime(CLOCK_MONOTONIC, &a);

    do {
        ret = fi_cq_read(cq, &comp, 1);
        if (ret > 0) {
            if (timeout >= 0)
                clock_gettime(CLOCK_MONOTONIC, &a);
            (*cur)++;
        } else if (ret < 0 && ret != -FI_EAGAIN) {
            return ret;
        } else if (timeout >= 0) {
            clock_gettime(CLOCK_MONOTONIC, &b);
            if ((b.tv_sec - a.tv_sec) > timeout) {
                fprintf(stderr, "%ds timeout expired\n", timeout);
                return -FI_ENODATA;
            }
        }
    } while (total != *cur);

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
        ret = -FI_EAGAIN;
    } else {
        ret = 0;
    }
    return ret;
}

/*
 * fi_cq_err_entry can be cast to any CQ entry format.
 */
static int rdma_fdwait_for_comp(ep_ctx_t* ep_ctx, struct fid_cq *cq, uint64_t *cur,
                uint64_t total, int timeout)
{
    struct fi_cq_err_entry comp;
    struct fid *fids[1];
    int fd, ret;

    fd = cq == ep_ctx->txcq ? ep_ctx->tx_fd : ep_ctx->rx_fd;
    fids[0] = &cq->fid;

    while (total != *cur) {
        ret = fi_trywait(ep_ctx->rdma_ctx->fabric, fids, 1);
        if (ret == FI_SUCCESS) {
            ret = rdma_poll_fd(fd, timeout);
            if (ret && ret != -FI_EAGAIN)
                return ret;
        }

        ret = fi_cq_read(cq, &comp, 1);
        if (ret > 0) {
            (*cur)++;
        } else if (ret < 0 && ret != -FI_EAGAIN) {
            return ret;
        }
    }

    return 0;
}

/*
 * fi_cq_err_entry can be cast to any CQ entry format.
 */
static int rdma_wait_for_comp(ep_ctx_t* ep_ctx, struct fid_cq *cq, uint64_t *cur,
                uint64_t total, int timeout)
{
    struct fi_cq_err_entry comp;
    int ret;

    while (total != *cur) {
        ret = fi_cq_sread(cq, &comp, 1, NULL, timeout);
        if (ret > 0) {
            (*cur)++;
        } else if (ret < 0 && ret != -FI_EAGAIN) {
            return ret;
        }
    }

    return 0;
}

int rdma_get_cq_comp(ep_ctx_t* ep_ctx, struct fid_cq *cq, uint64_t *cur,
            uint64_t total, int timeout)
{
    int ret;

    ret = rdma_read_cq(ep_ctx, cq, cur, total, timeout);

    if (ret) {
        if (ret == -FI_EAVAIL) {
            ret = rdma_cq_readerr(cq);
            (*cur)++;
        } else {
            RDMA_PRINTERR("rdma_get_cq_comp", ret);
        }
    }
    return ret;
}

int rdma_read_cq(ep_ctx_t* ep_ctx, struct fid_cq *cq, uint64_t *cur,
               uint64_t total, int timeout)
{
    int ret;

    switch (ep_ctx->rdma_ctx->comp_method) {
    case RDMA_COMP_SREAD:
    case RDMA_COMP_YIELD:
        ret = rdma_wait_for_comp(ep_ctx, cq, cur, total, timeout);
        break;
    case RDMA_COMP_WAIT_FD:
        ret = rdma_fdwait_for_comp(ep_ctx, cq, cur, total, timeout);
        break;
    default:
        ret = rdma_spin_for_comp(ep_ctx, cq, cur, total, timeout);
        break;
    }
    return ret;
}

int rdma_get_cq_fd(struct fid_cq *cq, int *fd, enum cq_comp_method method)
{
    int ret = FI_SUCCESS;

    if (cq && method == RDMA_COMP_WAIT_FD) {
        ret = fi_control(&cq->fid, FI_GETWAIT, fd);
        if (ret)
            RDMA_PRINTERR("fi_control(FI_GETWAIT)", ret);
    }

    return ret;
}

int rdma_cq_readerr(struct fid_cq *cq)
{
    struct fi_cq_err_entry cq_err;
    int ret;

    memset(&cq_err, 0, sizeof(cq_err));
    ret = fi_cq_readerr(cq, &cq_err, 0);
    if (ret < 0) {
        RDMA_PRINTERR("fi_cq_readerr", ret);
    } else {
        RDMA_CQ_ERR(cq, cq_err, NULL, 0);
        ret = -cq_err.err;
    }
    return ret;
}

// EQ not CQ
void eq_readerr(struct fid_eq *eq, const char *eq_str)
{
    struct fi_eq_err_entry eq_err;
    int rd;

    memset(&eq_err, 0, sizeof(eq_err));
    rd = fi_eq_readerr(eq, &eq_err, 0);
    if (rd != sizeof(eq_err)) {
        RDMA_PRINTERR("fi_eq_readerr", rd);
    } else {
        RDMA_EQ_ERR(eq, eq_err, NULL, 0);
    }
}
