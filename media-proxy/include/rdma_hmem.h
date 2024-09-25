/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Copyright (c) 2020 Intel Corporation.  All rights reserved.
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
 */

#ifndef _HMEM_H_
#define _HMEM_H_

#include <stdlib.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>

int rdma_ze_init(void);
int rdma_ze_cleanup(void);
int rdma_ze_alloc(uint64_t device, void **buf, size_t size);
int rdma_ze_alloc_host(void **buf, size_t size);
int rdma_ze_free(void *buf);
int rdma_ze_memset(uint64_t device, void *buf, int value, size_t size);
int rdma_ze_copy(uint64_t device, void *dst, const void *src, size_t size);

static inline int rdma_host_init(void) { return 0; }

static inline int rdma_host_cleanup(void) { return 0; }

static inline int rdma_host_alloc(uint64_t device, void **buffer, size_t size)
{
    *buffer = malloc(size);
    return !*buffer ? -ENOMEM : 0;
}

static inline int rdma_host_free(void *buf)
{
    free(buf);
    return 0;
}

static inline int rdma_host_memset(uint64_t device, void *buf, int value, size_t size)
{
    memset(buf, value, size);
    return 0;
}

static inline int rdma_host_memcpy(uint64_t device, void *dst, const void *src, size_t size)
{
    memcpy(dst, src, size);
    return 0;
}

int rdma_default_alloc_host(void **buf, size_t size);
int rdma_default_free_host(void *buf);

int rdma_hmem_init(enum fi_hmem_iface iface);
int rdma_hmem_cleanup(enum fi_hmem_iface iface);
int rdma_hmem_alloc(enum fi_hmem_iface iface, uint64_t device, void **buf, size_t size);
int rdma_hmem_alloc_host(enum fi_hmem_iface iface, void **buf, size_t size);
int rdma_hmem_free(enum fi_hmem_iface iface, void *buf);
int rdma_hmem_free_host(enum fi_hmem_iface iface, void *buf);
int rdma_hmem_memset(enum fi_hmem_iface iface, uint64_t device, void *buf, int value, size_t size);
int rdma_hmem_copy_to(enum fi_hmem_iface iface, uint64_t device, void *dst, const void *src,
                      size_t size);
int rdma_hmem_copy_from(enum fi_hmem_iface iface, uint64_t device, void *dst, const void *src,
                        size_t size);
int rdma_hmem_get_dmabuf_fd(enum fi_hmem_iface iface, void *buf, size_t len, int *fd,
                            uint64_t *offset);
int rdma_hmem_no_get_dmabuf_fd(void *buf, size_t len, int *fd, uint64_t *offset);

#endif /* _HMEM_H_ */
