/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// #include <signal.h>

#include "tcp_controller.h"
#include "mp_ctrl_proto.h"
#include <mcm_dp.h>

static volatile bool keepRunning = true;

typedef struct
{
    int sock;
    struct sockaddr address;
    int addr_len;
} connection_t;

typedef struct _control_context {
    ProxyContext* proxy_ctx;
    connection_t* conn;
} control_context;

void* msg_loop(void* ptr)
{
    int ret = 0;
    void* buffer = NULL;
    int len = 0;
    control_context* ctl_ctx = NULL;
    connection_t* conn = NULL;
    ProxyContext* proxy_ctx = NULL;
    long addr = 0;
    mcm_proxy_ctl_msg msg = {};
    uint32_t intf_id = 0;

    if (!ptr) {
        pthread_exit(0);
        INFO("Illegal Parameter.");
    }

    ctl_ctx = (control_context*)ptr;
    conn = ctl_ctx->conn;
    proxy_ctx = ctl_ctx->proxy_ctx;

    /* infinite loop */
    for (;;) {
        /* control message header */
        ret = read(conn->sock, &msg.header, sizeof(msg.header));
        if (ret <= 0) {
            break;
        }

        if (strncmp(msg.header.magic_word, "MCM", sizeof(msg.header.magic_word)) != 0) {
            continue;
        }
        if (msg.header.version != 0x01) {
            continue;
        }

        /* control command */
        ret = read(conn->sock, &msg.command, sizeof(msg.command));
        if (ret <= 0) {
            INFO("Fail to read control command.");
            break;
        }

        if (msg.command.data_len > 0) {
            /* read parameters */
            buffer = (char*)malloc((msg.command.data_len));
            if (buffer == NULL) {
                INFO("Outof Memory.");
                break;
            }
            len = msg.command.data_len;
            do {
                ret = read(conn->sock, buffer, len);
                if (ret <= 0) {
                    break;
                }
                len -= ret;
            } while (len > 0);
            if (len > 0) {
                INFO("Fail to read command parameters.");
                break;
            }
        }

        uint32_t session_id = 0;
        mcm_conn_param param = {};
        /* operation */
        switch (msg.command.inst) {
        case MCM_CREATE_SESSION:
            if (buffer == NULL) {
                INFO("Invalid parameters.");
                break;
            }
            memcpy(&param, buffer, sizeof(mcm_conn_param));
            if (param.type == is_tx) {
                ret = proxy_ctx->TxStart(&param);
            } else {
                ret = proxy_ctx->RxStart(&param);
            }

            if (ret >= 0) {
                if (write(conn->sock, &ret, sizeof(ret)) <= 0) {
                    INFO("Fail to return session id.");
                }
            } else {
                INFO("Fail to start MTL session.");
            }
            break;
        case MCM_QUERY_MEMIF_PATH:
            /*
            len = strlen("/run/mcm/media_proxy_0.sock");
            if (write(conn->sock, &len, sizeof(uint32_t)) <= 0) {
                INFO("Fail to return path length.");
            }
            if (write(conn->sock, "/run/mcm/media_proxy_0.sock", strlen("/run/mcm/media_proxy_0.sock")) <= 0) {
                INFO("Fail to return memif socket path.");
            }
            */
            break;
        case MCM_QUERY_MEMIF_ID:
            /*
            intf_id = 0;
            if (write(conn->sock, &intf_id, sizeof(intf_id)) <= 0) {
                INFO("Fail to return memif interface id.");
            }
            */
            break;
        case MCM_QUERY_MEMIF_PARAM:
            if (buffer == NULL || msg.command.data_len < 4) {
                INFO("Invalid parameters.");
                break;
            }
            session_id = *(uint32_t*)buffer;
            for (auto it : proxy_ctx->mStCtx) {
                if (it->id == session_id) {
                    /* return memif parameters. */
                    memif_conn_param param = {};
                    if (it->type == TX) {
                        switch (it->payload_type) {
                        case PAYLOAD_TYPE_ST22_VIDEO:
                            memcpy(&param.socket_args, &it->tx_st22p_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->tx_st22p_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        case PAYLOAD_TYPE_ST30_AUDIO:
                            memcpy(&param.socket_args, &it->tx_st30_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->tx_st30_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        case PAYLOAD_TYPE_ST40_ANCILLARY:
                            memcpy(&param.socket_args, &it->tx_st40_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->tx_st40_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        case PAYLOAD_TYPE_RTSP_VIDEO:
                            break;
                        case PAYLOAD_TYPE_ST20_VIDEO:
                            memcpy(&param.socket_args, &it->tx_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->tx_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        default:
                            INFO("Unknown session type.");
                            break;
                        }
                    } else {
                        switch (it->payload_type) {
                        case PAYLOAD_TYPE_ST22_VIDEO:
                            memcpy(&param.socket_args, &it->rx_st22p_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->rx_st22p_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        case PAYLOAD_TYPE_ST30_AUDIO:
                            memcpy(&param.socket_args, &it->rx_st30_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->rx_st30_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        case PAYLOAD_TYPE_ST40_ANCILLARY:
                            memcpy(&param.socket_args, &it->rx_st40_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->rx_st40_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        case PAYLOAD_TYPE_RTSP_VIDEO:
                            memcpy(&param.socket_args, &it->rx_udp_h264_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->rx_udp_h264_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        case PAYLOAD_TYPE_ST20_VIDEO:
                            memcpy(&param.socket_args, &it->rx_session->memif_socket_args, sizeof(memif_socket_args_t));
                            memcpy(&param.conn_args, &it->rx_session->memif_conn_args, sizeof(memif_conn_args_t));
                            break;
                        default:
                            INFO("Unknown session type.");
                            break;
                        }
                    }

                    if (param.conn_args.is_master) {
                        param.conn_args.is_master = 0;
                    } else {
                        param.conn_args.is_master = 1;
                    }

                    if (write(conn->sock, &param, sizeof(memif_conn_param)) <= 0) {
                        INFO("Fail to return path length.");
                    }
                    break;
                }
            }
            break;
        case MCM_DESTROY_SESSION:
            if (buffer == NULL || msg.command.data_len < 4) {
                INFO("Invalid parameters.");
                break;
            }
            session_id = *(uint32_t*)buffer;
            for (auto it : proxy_ctx->mStCtx) {
                if (it->id == session_id) {
                    if (it->type == TX) {
                        proxy_ctx->TxStop(session_id);
                    } else {
                        proxy_ctx->RxStop(session_id);
                    }

                    break;
                }
            }
            break;
        default:
            break;
        }

        if (buffer != NULL) {
            free(buffer);
            buffer = NULL;
        }
    }

    addr = (long)((struct sockaddr_in*)&conn->address)->sin_addr.s_addr;
    INFO("Disconnect with %d.%d.%d.%d",
        (int)((addr)&0xff), (int)((addr >> 8) & 0xff),
        (int)((addr >> 16) & 0xff), (int)((addr >> 24) & 0xff));

    /* Clean-up all sessions. */
    // for (auto it : proxy_ctx->mStCtx) {
    //     if (it->type == TX) {
    //         proxy_ctx->TxStop(it->id);
    //     } else {
    //         proxy_ctx->RxStop(it->id);
    //     }
    // }

    /* close socket and clean up */
    close(conn->sock);
    free(conn);
    free(ctl_ctx);

    pthread_exit(0);
}

// void intHandler(int dummy)
// {
//     keepRunning = 0;
// }

void RunTCPServer(ProxyContext* ctx)
{
    int sock = -1;
    struct sockaddr_in address;
    int port = 0;
    connection_t* connection = NULL;
    control_context* ctl_ctx = NULL;
    pthread_t thread;

    port = ctx->getTCPListenPort();
    if (port <= 0) {
        INFO("Illegal TCP listen address");
        return;
    }

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock <= 0) {
        fprintf(stderr, "error: cannot create socket\n");
        return;
    }

    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "error: cannot bind socket to port %d: %s\n", port, strerror(errno));
        return;
    }

    /* listen on port */
    if (listen(sock, 5) < 0) {
        fprintf(stderr, "error: cannot listen on port\n");
        return;
    }

    INFO("TCP Server listening on %s", ctx->getTCPListenAddress().c_str());

    // signal(SIGINT, intHandler);

    while (keepRunning) {
        /* accept incoming connections */
        connection = (connection_t*)malloc(sizeof(connection_t));
        connection->sock = accept(sock, &connection->address, (socklen_t*)&connection->addr_len);
        if (connection->sock <= 0) {
            free(connection);
        } else {
            /* start a new thread but do not wait for it */
            ctl_ctx = (control_context*)malloc(sizeof(control_context));
            ctl_ctx->proxy_ctx = ctx;
            ctl_ctx->conn = connection;
            pthread_create(&thread, 0, msg_loop, (void*)ctl_ctx);
            pthread_detach(thread);
        }
    }

    INFO("TCP Server Quit: %s", ctx->getTCPListenAddress().c_str());

    return;
}
