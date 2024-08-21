/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MCM_COMMON_H__
#define __MCM_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <mcm_dp.h>

int mcm_parse_conn_param(mcm_conn_param *param, transfer_type type,
                         char *ip_addr, char *port, char *protocol_type,
                         char *payload_type, char *socket_name, int interface_id);

#ifdef __cplusplus
}
#endif

#endif /* __MCM_COMMON_H__ */
