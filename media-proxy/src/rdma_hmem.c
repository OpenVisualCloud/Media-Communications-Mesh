/*
 * Copyright (c) 2020 Intel Corporation.  All rights reserved.
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates.
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

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "rdma_hmem.h"

static bool hmem_initialized = false;

struct rdma_hmem_ops {
    int (*init)(void);
    int (*cleanup)(void);
    int (*alloc)(uint64_t device, void **buf, size_t size);
    int (*alloc_host)(void **buf, size_t size);
    int (*free)(void *buf);
    int (*free_host)(void *buf);
    int (*mem_set)(uint64_t device, void *buf, int value, size_t size);
    int (*copy_to_hmem)(uint64_t device, void *dst, const void *src,
                size_t size);
    int (*copy_from_hmem)(uint64_t device, void *dst, const void *src,
                  size_t size);
    int (*get_dmabuf_fd)(void *buf, size_t len,
                 int *fd, uint64_t *offset);
};

static struct rdma_hmem_ops hmem_ops[] = {
    [FI_HMEM_SYSTEM] = {
        .init = rdma_host_init,
        .cleanup = rdma_host_cleanup,
        .alloc = rdma_host_alloc,
        .alloc_host = rdma_default_alloc_host,
        .free = rdma_host_free,
        .free_host = rdma_default_free_host,
        .mem_set = rdma_host_memset,
        .copy_to_hmem = rdma_host_memcpy,
        .copy_from_hmem = rdma_host_memcpy,
        .get_dmabuf_fd = rdma_hmem_no_get_dmabuf_fd,
    },
    // More devices could be added
};

int rdma_hmem_init(enum fi_hmem_iface iface)
{
    int ret;

    ret = hmem_ops[iface].init();
    if (ret == FI_SUCCESS)
        hmem_initialized = true;

    return ret;
}

int rdma_hmem_cleanup(enum fi_hmem_iface iface)
{
    int ret = FI_SUCCESS;

    if (hmem_initialized) {
        ret = hmem_ops[iface].cleanup();
        if (ret == FI_SUCCESS)
            hmem_initialized = false;
    }

    return ret;
}

int rdma_hmem_alloc(enum fi_hmem_iface iface, uint64_t device, void **buf,
          size_t size)
{
    return hmem_ops[iface].alloc(device, buf, size);
}

int rdma_default_alloc_host(void **buf, size_t size)
{
    *buf = malloc(size);
    return (*buf == NULL) ? -FI_ENOMEM : 0;
}

int rdma_default_free_host(void *buf)
{
    free(buf);
    return 0;
}

int rdma_hmem_alloc_host(enum fi_hmem_iface iface, void **buf,
               size_t size)
{
    return hmem_ops[iface].alloc_host(buf, size);
}

int rdma_hmem_free(enum fi_hmem_iface iface, void *buf)
{
    return hmem_ops[iface].free(buf);
}

int rdma_hmem_free_host(enum fi_hmem_iface iface, void *buf)
{
    return hmem_ops[iface].free_host(buf);
}

int rdma_hmem_memset(enum fi_hmem_iface iface, uint64_t device, void *buf,
           int value, size_t size)
{
    return hmem_ops[iface].mem_set(device, buf, value, size);
}

int rdma_hmem_copy_to(enum fi_hmem_iface iface, uint64_t device, void *dst,
            const void *src, size_t size)
{
    return hmem_ops[iface].copy_to_hmem(device, dst, src, size);
}

int rdma_hmem_copy_from(enum fi_hmem_iface iface, uint64_t device, void *dst,
              const void *src, size_t size)
{
    return hmem_ops[iface].copy_from_hmem(device, dst, src, size);
}

int rdma_hmem_get_dmabuf_fd(enum fi_hmem_iface iface,
              void *buf, size_t len,
              int *fd, uint64_t *offset)
{
    return hmem_ops[iface].get_dmabuf_fd(buf, len, fd, offset);
}

int rdma_hmem_no_get_dmabuf_fd(void *buf, size_t len,
                  int *fd, uint64_t *offset)
{
    return -FI_ENOSYS;
}
