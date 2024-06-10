/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MEDIA_PROXY_CTRL_H
#define __MEDIA_PROXY_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "memif_impl.h"
#include "libmemif.h"
#include "mcm_dp.h"

int get_media_proxy_addr(mcm_dp_addr* proxy_addr);

int open_socket(mcm_dp_addr* proxy_addr);

void close_socket(int sockfd);

int media_proxy_create_session(int sockfd, mcm_conn_param* param, uint32_t* session_id);

void media_proxy_destroy_session(mcm_conn_context* pctx);

int media_proxy_query_interface(int sockfd, uint32_t session_id, mcm_conn_param* param, memif_conn_param* memif_conn_args);

#ifdef __cplusplus
}
#endif

#endif /* __MEDIA_PROXY_CTRL_H */
