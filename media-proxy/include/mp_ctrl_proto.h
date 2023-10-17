/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MP_CTRL_PROTO_H
#define __MP_CTRL_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Media proxy control commands. */
#define MCM_CREATE_SESSION      1
#define MCM_DESTROY_SESSION     2
#define MCM_QUERY_MEMIF_PATH    3
#define MCM_QUERY_MEMIF_ID      4
#define MCM_QUERY_MEMIF_PARAM   5

typedef struct _msg_header {
    char magic_word[3];
    uint8_t version;
} msg_header;

typedef struct _ctl_cmd {
    uint16_t inst;
    uint16_t data_len;
} ctl_cmd;

typedef struct _mcm_proxy_ctrl_msg {
    msg_header header;
    ctl_cmd command;
    void* data;
} mcm_proxy_ctl_msg;


#ifdef __cplusplus
}
#endif

#endif /* __MP_CTRL_PROTO_H */
