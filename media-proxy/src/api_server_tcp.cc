/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
*/

#include <bsd/string.h>
#include "api_server_tcp.h"
#include <mcm_dp.h>
#include <signal.h>
#include <netinet/ip.h>

static volatile bool keepRunning = true;

typedef struct {
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
    int len = 0;
    char* buffer = NULL;
    bool sessionKeepRunning = true;
    control_context* ctl_ctx = NULL;
    connection_t* conn = NULL;
    ProxyContext* proxy_ctx = NULL;
    long addr = 0;
    mcm_proxy_ctl_msg msg = {};
    uint32_t session_id = 0;
    uint32_t intf_id = 0;

    if (!ptr) {
        ERROR("msg_loop(void* ptr): Illegal Parameter, ptr==NULL.");
        pthread_exit(0);
    }

    ctl_ctx = (control_context*)ptr;
    conn = ctl_ctx->conn;
    proxy_ctx = ctl_ctx->proxy_ctx;

    /* infinite loop */
    do {
        /* read control message header */
        ret = read(conn->sock, &msg.header, sizeof(msg.header));
        if (ret <= 0) {
            break;
        }

        if (msg.header.magic_word != *(uint32_t*)HEADER_MAGIC_WORD) {
            ERROR("Header Data Mismatch: Incorrect magic word.");
            continue;
        }
        if (msg.header.version != HEADER_VERSION) {
            ERROR("Header Data Mismatch: Incorrect version of client.");
            continue;
        }

        /* control command */
        ret = read(conn->sock, &msg.command, sizeof(msg.command));
        if (ret <= 0) {
            INFO("Failed to read control command.");
            break;
        }

        if (msg.command.data_len > 0) {
            /* read parameters */
            buffer = (char*)malloc(msg.command.data_len);
            if (buffer == NULL) {
                ERROR("(char*)malloc(msg.command.data_len) in msg_loop() failed. Out of Memory.");
                continue;
            } else {
                ret = 0;
                int bytesRead = 0;
                while (bytesRead < msg.command.data_len && ret >= 0) {
                    ret = read(conn->sock, buffer + bytesRead, msg.command.data_len - bytesRead);
                    if (ret >= 0) bytesRead += ret;
                }
                if (bytesRead < msg.command.data_len) {
                    ERROR("Read socket failed: Failed to read all command parameters.");
                    free(buffer);
                    buffer = NULL;
                    continue;
                }
            }
        }

        mcm_conn_param param = { };

        /* operation */
        switch (msg.command.inst) {
        case MCM_CREATE_SESSION:
            DEBUG("MCM_CREATE_SESSION: Case entry.");
            if (buffer == NULL) {
                INFO("MCM_CREATE_SESSION: Invalid parameters, buffer is NULL.");
                break;
            }
            memcpy(&param, buffer, sizeof(mcm_conn_param));
            if (param.type == is_tx) {
                ret = proxy_ctx->TxStart(&param);
            } else {
                ret = proxy_ctx->RxStart(&param);
            }

            if (ret >= 0) {
                session_id = (uint32_t)ret;
                if (write(conn->sock, &session_id, sizeof(session_id)) <= 0) {
                    ERROR("MCM_CREATE_SESSION: Return session id error, failed to write socket.");
                }
            } else {
                ERROR("MCM_CREATE_SESSION: Failed to start MTL session.");
            }
            break;
        case MCM_QUERY_MEMIF_PATH:
            DEBUG("MCM_QUERY_MEMIF_PATH: Case entry.");
            /* TODO: return memif socket path */
            break;
        case MCM_QUERY_MEMIF_ID:
            DEBUG("MCM_QUERY_MEMIF_ID: Case entry.");
            /* TODO: return memdif ID */
            break;
        case MCM_QUERY_MEMIF_PARAM:
            DEBUG("MCM_QUERY_MEMIF_PARAM: Case entry.");
            if (buffer == NULL || msg.command.data_len < 4) {
                INFO("Invalid parameters.");
                break;
            }
            session_id = *(uint32_t*)buffer;
            for (auto it : proxy_ctx->mDpCtx) {
                if (it->id == session_id) {
                    /* return memif parameters. */
                    memif_conn_param param = { };

                    memcpy(&param.socket_args, &it->memif_socket_args, sizeof(memif_socket_args_t));
                    memcpy(&param.conn_args, &it->memif_conn_args, sizeof(memif_conn_args_t));

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
            DEBUG("MCM_DESTROY_SESSION: Case entry.");
            if (buffer == NULL || msg.command.data_len < 4) {
                INFO("Invalid parameters.");
                break;
            }
            session_id = *(uint32_t*)buffer;
            if(!proxy_ctx->Stop(session_id)){
                sessionKeepRunning = false;
            }
            break;
        default:
            DEBUG("UNKNOWN_CASE: Default case entry.");
            break;
        }

        if (buffer != NULL) {
            free(buffer);
            buffer = NULL;
        }
    } while (keepRunning && sessionKeepRunning);

    addr = (long)((struct sockaddr_in*)&conn->address)->sin_addr.s_addr;
    INFO("Disconnect with %d.%d.%d.%d",
        (int)((addr)&0xff), (int)((addr >> 8) & 0xff),
        (int)((addr >> 16) & 0xff), (int)((addr >> 24) & 0xff));

    if(session_id > 0) {
        proxy_ctx->Stop(session_id);
    }

    /* close socket and clean up */
    close(conn->sock);
    free(conn);
    free(ctl_ctx);

    pthread_exit(0);
}

void handleSignals(int sig_num)
{
    keepRunning = false;
    exit(0);
}

void registerSignals()
{
    signal(SIGINT, handleSignals);
    signal(SIGTERM, handleSignals);
    signal(SIGKILL, handleSignals);
}

void RunTCPServer(ProxyContext* ctx)
{
    int sock = -1;
    struct sockaddr_in address;
    int port = 0;
    connection_t* connection = NULL;
    control_context* ctl_ctx = NULL;
    pthread_t thread;
    const int enable = 1;

    port = ctx->getTCPListenPort();
    if (port <= 0) {
        INFO("Illegal TCP listen address");
        return;
    }

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "error: cannot create socket\n");
        return;
    }

    /* Workaround to allow media_proxy to listen on the same port after termination and starting again */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        fprintf(stderr, "error: cannot set SO_REUSEADDR");
        close(sock);
        return;
    }

    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "error: cannot bind socket to port %d: %s\n", port, strerror(errno));
        close(sock);
        return;
    }

    /* listen on port and set 500 for 1K IPC sessions burst */
    if (listen(sock, 500) < 0) {
        fprintf(stderr, "error: cannot listen on port\n");
        return;
    }

    INFO("TCP Server listening on %s", ctx->getTCPListenAddress().c_str());
    registerSignals();

    do {
        /* accept incoming connections */
        connection = (connection_t*)malloc(sizeof(connection_t));
        if (connection) {
            memset(connection, 0x0, sizeof(connection_t));
            connection->sock = accept(sock, &connection->address, (socklen_t*)&connection->addr_len);
            if (connection->sock > 0) {
                /* start a new thread but do not wait for it */
                ctl_ctx = (control_context*)malloc(sizeof(control_context));
                if (ctl_ctx) {
                    memset(ctl_ctx, 0x0, sizeof(control_context));
                    ctl_ctx->proxy_ctx = ctx;
                    ctl_ctx->conn = connection;
                    if (pthread_create(&thread, 0, msg_loop, (void*)ctl_ctx) == 0) {
                        pthread_detach(thread);
                    } else {
                        free(connection);
                        free(ctl_ctx);
                    }
                } else {
                    free(connection);
                }
            } else {
                free(connection);
            }
        }
    } while (keepRunning);

    INFO("TCP Server Quit: %s", ctx->getTCPListenAddress().c_str());

    return;
}
