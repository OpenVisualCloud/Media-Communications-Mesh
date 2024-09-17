/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arpa/inet.h>
#include <bsd/string.h>
#include <stdlib.h>
#include <unistd.h>

#include "logger.h"
#include "media_proxy_ctrl.h"

int get_media_proxy_addr(MeshClient mc, mcm_dp_addr* proxy_addr)
{
    char* penv_val = NULL;
    const char DEFAULT_PROXY_IP[] = "127.0.0.1";
    const char DEFAULT_PROXY_PORT[] = "8002";

    if (proxy_addr == NULL) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal Parameter.");
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

int open_socket(MeshClient mc, mcm_dp_addr* proxy_addr)
{
    int sockfd = -1;
    struct sockaddr_in srvaddr = {};

    /* create socket and verification */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        mesh_log(mc, MESH_LOG_ERROR, "Failed to create socket.");
        return -1;
    }

    /* set IP and port */
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = inet_addr(proxy_addr->ip);
    srvaddr.sin_port = htons(atoi(proxy_addr->port));

    /* connect to media-proxy socket. */
    if (connect(sockfd, (struct sockaddr*)&srvaddr, sizeof(srvaddr)) != 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Failed to connect to media-proxy socket.");
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

int media_proxy_create_session(MeshClient mc, mcm_conn_param *param, int sockfd, uint32_t* session_id)
{
    ssize_t ret = 0;
    mcm_proxy_ctl_msg msg = {};


    if (sockfd <= 0 || param == NULL || session_id == NULL) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal Parameters.");
        return -1;
    }

    /* intialize message header. */
    msg.header.magic_word = *(uint32_t*)HEADER_MAGIC_WORD;
    msg.header.version = HEADER_VERSION;

    msg.command.inst = MCM_CREATE_SESSION;
    msg.command.data_len = sizeof(MeshConnectionConfig);
    msg.data = param;

    /* header */
    if (write(sockfd, &msg.header, sizeof(msg.header)) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send message header failed.");
        return -1;
    }

    /* command */
    if (write(sockfd, &msg.command, sizeof(msg.command)) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send command failed.");
        return -1;
    }

    /* parameters */
    if (write(sockfd, msg.data, msg.command.data_len) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send parameters failed.");
        return -1;
    }

    /* get session id */
    ret = read(sockfd, session_id, sizeof(uint32_t));
    if (ret <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Receive session id failed.");
        log_info("Session ID can not be received: %d", *session_id);
        return -1;
    }
    log_info("Session ID: %d", *session_id);

    return 0;
}

int media_proxy_query_interface(MeshClient mc, int sockfd, uint32_t session_id, mcm_conn_param* param, memif_conn_param* memif_conn_args)
{
    ssize_t ret = 0;
    mcm_proxy_ctl_msg msg = {};

    if (sockfd < 0 || memif_conn_args == NULL) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal Parameters.");
        return -1;
    }

    /* intialize message header. */
    msg.header.magic_word = *(uint32_t*)HEADER_MAGIC_WORD;
    msg.header.version = HEADER_VERSION;

    /* memif parameters */
    msg.command.inst = MCM_QUERY_MEMIF_PARAM;
    msg.command.data_len = sizeof(session_id);
    msg.data = &session_id;

    /* header */
    if (write(sockfd, &msg.header, sizeof(msg.header)) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send message header failed.");
        return -1;
    }

    /* command */
    if (write(sockfd, &msg.command, sizeof(msg.command)) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send command failed.");
        return -1;
    }

    /* parameters */
    if (write(sockfd, msg.data, msg.command.data_len) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send parameters failed.");
        return -1;
    }

    /* get result */
    ret = read(sockfd, memif_conn_args, sizeof(memif_conn_param));
    if (ret <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Receive interface id failed.");
        return -1;
    }

    return 0;
}

void media_proxy_destroy_session(MeshClient mc, MeshConnection conn)
{
    int sockfd = 0;
    uint32_t session_id = 0;
    mcm_proxy_ctl_msg msg = {};
    MeshConnectionConfig *conn_conf= (MeshConnectionConfig*) conn;    

    if (conn == NULL) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal Parameters.");
        return;
    }

    sockfd = conn_conf->proxy_sockfd;
    session_id = conn_conf->session_id;

    /* intialize message header. */
    msg.header.magic_word = *(uint32_t*)HEADER_MAGIC_WORD;
    msg.header.version = HEADER_VERSION;

    msg.command.inst = MCM_DESTROY_SESSION;
    msg.command.data_len = sizeof(session_id);
    msg.data = &session_id;

    /* header */
    if (write(sockfd, &msg.header, sizeof(msg.header)) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send message header failed.");
        return;
    }

    /* command */
    if (write(sockfd, &msg.command, sizeof(msg.command)) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send command failed.");
        return;
    }

    /* parameters */
    if (write(sockfd, msg.data, msg.command.data_len) <= 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Send parameters failed.");
        return;
    }

    return;
}
