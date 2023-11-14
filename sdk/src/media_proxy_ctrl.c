/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arpa/inet.h>
#include <bsd/string.h>
#include <stdlib.h>
#include <unistd.h>

#include "logger.h"
#include "media_proxy_ctrl.h"
#include "mp_ctrl_proto.h"

int get_media_proxy_addr(mcm_dp_addr* proxy_addr)
{
    char* penv_val = NULL;
    const char DEFAULT_PROXY_IP[] = "127.0.0.1";
    const char DEFAULT_PROXY_PORT[] = "8002";

    if (proxy_addr == NULL) {
        log_error("Illegal Parameter.");
        return -1;
    }

    if ((penv_val = getenv("MCM_MEDIA_PROXY_IP")) != NULL) {
        strlcpy(proxy_addr->ip, penv_val, sizeof(proxy_addr->ip) - 1);
    } else {
        log_info("Set default media-proxy IP address: %s", DEFAULT_PROXY_IP);
        strlcpy(proxy_addr->ip, DEFAULT_PROXY_IP, sizeof(proxy_addr->ip));
    }

    if ((penv_val = getenv("MCM_MEDIA_PROXY_PORT")) != NULL) {
        strlcpy(proxy_addr->port, penv_val, sizeof(proxy_addr->port) - 1);
    } else {
        log_info("Set default media-proxy port: %s:", DEFAULT_PROXY_PORT);
        strlcpy(proxy_addr->port, DEFAULT_PROXY_PORT, sizeof(proxy_addr->port));
    }

    return 0;
}

int open_socket(mcm_dp_addr* proxy_addr)
{
    int sockfd = -1;
    struct sockaddr_in srvaddr = {};

    /* create socket and verification */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        log_error("Failed to create socket.");
        return -1;
    }

    /* set IP and port */
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = inet_addr(proxy_addr->ip);
    srvaddr.sin_port = htons(atoi(proxy_addr->port));

    /* connect to media-proxy socket. */
    if (connect(sockfd, (struct sockaddr*)&srvaddr, sizeof(srvaddr)) != 0) {
        log_error("Failed to connect to media-proxy socket.");
        close(sockfd);
        return -1;
    }

    log_info("Connected to media-proxy.");

    return sockfd;
}

void close_socket(int sockfd)
{
    close(sockfd);

    return;
}

int media_proxy_create_session(int sockfd, mcm_conn_param* param, uint32_t* session_id)
{
    ssize_t ret = 0;
    mcm_proxy_ctl_msg msg = {};

    if (sockfd <= 0 || param == NULL || session_id == NULL) {
        log_error("Illegal Parameters.");
        return -1;
    }

    /* intialize message header. */
    msg.header.magic_word[0] = 'M';
    msg.header.magic_word[1] = 'C';
    msg.header.magic_word[2] = 'M';
    msg.header.version = 0x01;

    msg.command.inst = MCM_CREATE_SESSION;
    msg.command.data_len = sizeof(mcm_conn_param);
    msg.data = param;

    /* header */
    if (write(sockfd, &msg.header, sizeof(msg.header)) <= 0) {
        log_error("Send message header failed.");
        return -1;
    }

    /* command */
    if (write(sockfd, &msg.command, sizeof(msg.command)) <= 0) {
        log_error("Send command failed.");
        return -1;
    }

    /* parameters */
    if (write(sockfd, msg.data, msg.command.data_len) <= 0) {
        log_error("Send parameters failed.");
        return -1;
    }

    /* get session id */
    ret = read(sockfd, session_id, sizeof(uint32_t));
    if (ret <= 0) {
        log_error("Receive session id failed.");
        log_info("Session ID can not be received: %d", *session_id);
        return -1;
    }
    log_info("Session ID: %d", *session_id);

    return 0;
}

// #define FRAME_SIZE 5184000          // yuv422p10be (1920*1080*2.5)
// #define FRAME_SIZE 3110400          // yuv420 (nv12) (1920*1080*3/2)
int media_proxy_query_interface(int sockfd, uint32_t session_id, mcm_conn_param* param, memif_conn_param* memif_conn_args)
{
    ssize_t ret = 0;
    mcm_proxy_ctl_msg msg = {};
    // size_t len = 0;
    // char* buf = NULL;
    // char type_str[8] = {};
    // static int mSessionCount = 0;

    if (sockfd < 0 || memif_conn_args == NULL) {
        log_error("Illegal Parameters.");
        return -1;
    }

    /* intialize message header. */
    msg.header.magic_word[0] = 'M';
    msg.header.magic_word[1] = 'C';
    msg.header.magic_word[2] = 'M';
    msg.header.version = 0x01;

    /* memif parameters */
    msg.command.inst = MCM_QUERY_MEMIF_PARAM;
    msg.command.data_len = sizeof(session_id);
    msg.data = &session_id;

    /* header */
    if (write(sockfd, &msg.header, sizeof(msg.header)) <= 0) {
        log_error("Send message header failed.");
        return -1;
    }

    /* command */
    if (write(sockfd, &msg.command, sizeof(msg.command)) <= 0) {
        log_error("Send command failed.");
        return -1;
    }

    /* parameters */
    if (write(sockfd, msg.data, msg.command.data_len) <= 0) {
        log_error("Send parameters failed.");
        return -1;
    }

    /* get result */
    ret = read(sockfd, memif_conn_args, sizeof(memif_conn_param));
    if (ret <= 0) {
        log_error("Receive interface id failed.");
        return -1;
    }

    /* debug code */
    // memif_conn_args->conn_args.buffer_size = FRAME_SIZE;
    // memif_conn_args->conn_args.log2_ring_size = 2;
    // memif_conn_args->conn_args.is_master = 0;

    // msg.command.inst = MCM_QUERY_MEMIF_PATH;
    // msg.command.data_len = sizeof(session_id);
    // msg.data = &session_id;

    /* memif path */
    /* header */
    // if (write(sockfd, &msg.header, sizeof(msg.header)) <= 0) {
    //     log_error("Send message header failed.");
    //     return -1;
    // }

    // /* command */
    // if (write(sockfd, &msg.command, sizeof(msg.command)) <= 0) {
    //     log_error("Send command failed.");
    //     return -1;
    // }

    // /* parameters */
    // if (write(sockfd, msg.data, msg.command.data_len) <= 0) {
    //     log_error("Send parameters failed.");
    //     return -1;
    // }

    // /* get result */
    // if (read(sockfd, &len, sizeof(len)) <= 0) {
    //     log_error("Receive memif path failed.");
    //     return -1;
    // }
    // buf = calloc(len, 1);
    // if (read(sockfd, buf, len) <= 0) {
    //     log_error("Receive memif path failed.");
    //     return -1;
    // }

    // /* interface id */
    // msg.command.inst = MCM_QUERY_MEMIF_ID;
    // msg.command.data_len = sizeof(session_id);
    // msg.data = &session_id;

    // /* header */
    // if (write(sockfd, &msg.header, sizeof(msg.header)) <= 0) {
    //     log_error("Send message header failed.");
    //     return -1;
    // }

    // /* command */
    // if (write(sockfd, &msg.command, sizeof(msg.command)) <= 0) {
    //     log_error("Send command failed.");
    //     return -1;
    // }

    // /* parameters */
    // if (write(sockfd, msg.data, msg.command.data_len) <= 0) {
    //     log_error("Send parameters failed.");
    //     return -1;
    // }

    // /* get result */
    // if (read(sockfd, &memif_conn_args->conn_args.interface_id, sizeof(memif_conn_args->conn_args.interface_id)) <= 0) {
    //     log_error("Receive interface id failed.");
    //     return -1;
    // }

    // if (param->type == is_tx) {
    //     strncpy(type_str, "tx", sizeof(type_str));
    // } else {
    //     strncpy(type_str, "rx", sizeof(type_str));
    // }

    // snprintf(memif_conn_args->socket_args.app_name, sizeof(memif_conn_args->socket_args.app_name),
    //     "memif_%s_%d", type_str, mSessionCount);
    // snprintf((char*)memif_conn_args->conn_args.interface_name, sizeof(memif_conn_args->conn_args.interface_name),
    //     "memif_%s_%d", type_str, mSessionCount);
    // snprintf(memif_conn_args->socket_args.path, sizeof(memif_conn_args->socket_args.path),
    //     "/run/mcm/media_proxy_%s_%d.sock", type_str, mSessionCount);

    // mSessionCount++;

    return 0;
}

void media_proxy_destroy_session(mcm_conn_context* pctx)
{
    int sockfd = 0;
    uint32_t session_id = 0;
    mcm_proxy_ctl_msg msg = {};

    if (pctx == NULL) {
        log_error("Illegal Parameters.");
        return;
    }

    sockfd = pctx->proxy_sockfd;
    session_id = pctx->session_id;

    /* intialize message header. */
    msg.header.magic_word[0] = 'M';
    msg.header.magic_word[1] = 'C';
    msg.header.magic_word[2] = 'M';
    msg.header.version = 0x01;

    msg.command.inst = MCM_DESTROY_SESSION;
    msg.command.data_len = sizeof(session_id);
    msg.data = &session_id;

    /* header */
    if (write(sockfd, &msg.header, sizeof(msg.header)) <= 0) {
        log_error("Send message header failed.");
        return;
    }

    /* command */
    if (write(sockfd, &msg.command, sizeof(msg.command)) <= 0) {
        log_error("Send command failed.");
        return;
    }

    /* parameters */
    if (write(sockfd, msg.data, msg.command.data_len) <= 0) {
        log_error("Send parameters failed.");
        return;
    }

    return;
}
