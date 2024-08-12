/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __RDMA_DEV_H
#define __RDMA_DEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mcm_dp.h>

#include "app_platform.h"
#include "shm_memif.h" /* share memory */
#include "utils.h"

#define RDMA_PRINTERR(call, retv)						\
	do {								\
		int saved_errno = errno;				\
		fprintf(stderr, call "(): %s:%d, ret=%d (%s)\n",	\
			__FILE__, __LINE__, (int) (retv),		\
			fi_strerror((int) -(retv)));			\
		errno = saved_errno;					\
	} while (0)

#define RDMA_LOG(level, fmt, ...)						\
	do {								\
		int saved_errno = errno;				\
		fprintf(stderr, "[%s] fabtests:%s:%d: " fmt "\n",	\
			level, __FILE__, __LINE__, ##__VA_ARGS__);	\
		errno = saved_errno;					\
	} while (0)

#define RDMA_ERR(fmt, ...) RDMA_LOG("error", fmt, ##__VA_ARGS__)
#define RDMA_WARN(fmt, ...) RDMA_LOG("warn", fmt, ##__VA_ARGS__)

#if ENABLE_DEBUG
#define RDMA_DEBUG(fmt, ...) RDMA_LOG("debug", fmt, ##__VA_ARGS__)
#else
#define RDMA_DEBUG(fmt, ...)
#endif

#define RDMA_EQ_ERR(eq, entry, buf, len)					\
	RDMA_ERR("eq_readerr %d (%s), provider errno: %d (%s)",		\
		entry.err, fi_strerror(entry.err),			\
		entry.prov_errno, fi_eq_strerror(eq, entry.prov_errno,	\
						 entry.err_data,	\
						 buf, len))		\

#define RDMA_CQ_ERR(cq, entry, buf, len)					\
	RDMA_ERR("cq_readerr %d (%s), provider errno: %d (%s)",		\
		entry.err, fi_strerror(entry.err),			\
		entry.prov_errno, fi_cq_strerror(cq, entry.prov_errno,	\
						 entry.err_data,	\
						 buf, len))		\

#define RDMA_CLOSE_FID(fd)						\
	do {								\
		int ret;						\
		if ((fd)) {						\
			ret = fi_close(&(fd)->fid);			\
			if (ret)					\
				RDMA_ERR("fi_close: %s(%d) fid %d",	\
					fi_strerror(-ret), 		\
					ret,				\
					(int) (fd)->fid.fclass);	\
			fd = NULL;					\
		}							\
	} while (0)

#define RDMA_CLOSEV_FID(fd, cnt)			\
	do {					\
		int i;				\
		if (!(fd))			\
			break;			\
		for (i = 0; i < (cnt); i++) {	\
			RDMA_CLOSE_FID((fd)[i]);	\
		}				\
	} while (0)

#define RDMA_EP_BIND(ep, fd, flags)					\
	do {								\
		int ret;						\
		if ((fd)) {						\
			ret = fi_ep_bind((ep), &(fd)->fid, (flags));	\
			if (ret) {					\
				RDMA_PRINTERR("fi_ep_bind", ret);		\
				return ret;				\
			}						\
		}							\
	} while (0)

enum cq_comp_method {
	RDMA_COMP_SPIN = 0,
	RDMA_COMP_SREAD,
	RDMA_COMP_WAITSET,
	RDMA_COMP_WAIT_FD,
	RDMA_COMP_YIELD,
};

typedef struct{
  enum cq_comp_method comp_method;

  struct fid_fabric *fabric;
  struct fid_domain *domain;
  struct fi_info* info;
} libfabric_ctx;

typedef struct{
  // Not used
} libfabric_cfg;

int rdma_init(libfabric_ctx** ctx);

/* Deinitialize RDMA */
int rdma_deinit(libfabric_ctx** ctx);

#ifdef __cplusplus
}
#endif

#endif /* __RDMA_DEV_H */
