/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "memif_impl.h"
#include "libmemif.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void print_memif_details(memif_conn_handle_t conn)
{
    printf("MEMIF DETAILS\n");
    printf("==============================\n");

    memif_details_t md;
    memset(&md, 0, sizeof(md));
    ssize_t buflen = 2048;
    char* buf = (char*)malloc(buflen);
    memset(buf, 0, buflen);
    int err, e;

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
//TBD        mesh_log(mc, MESH_LOG_INFO, "%s", memif_strerror(err));
        if (err == MEMIF_ERR_NOCONN) {
            free(buf);
            return;
        }
    }

    printf("\tinterface name: %s\n", (char*)md.if_name);
    printf("\tapp name: %s\n", (char*)md.inst_name);
    printf("\tremote interface name: %s\n", (char*)md.remote_if_name);
    printf("\tremote app name: %s\n", (char*)md.remote_inst_name);
    printf("\tid: %u\n", md.id);
    printf("\tsecret: %s\n", (char*)md.secret);
    printf("\trole: ");
    if (md.role)
        printf("slave\n");
    else
        printf("master\n");
    printf("\tmode: ");
    switch (md.mode) {
    case 0:
        printf("ethernet\n");
        break;
    case 1:
        printf("ip\n");
        break;
    case 2:
        printf("punt/inject\n");
        break;
    default:
        printf("unknown\n");
        break;
    }
    printf("\tsocket path: %s\n", (char*)md.socket_path);
    printf("\tregions num: %d\n", md.regions_num);
    for (int i = 0; i < md.regions_num; i++) {
        printf("\t\tregions idx: %d\n", md.regions[i].index);
        printf("\t\tregions addr: %p\n", md.regions[i].addr);
        printf("\t\tregions size: %d\n", md.regions[i].size);
        printf("\t\tregions ext: %d\n", md.regions[i].is_external);
    }
    printf("\trx queues:\n");
    for (e = 0; e < md.rx_queues_num; e++) {
        printf("\t\tqueue id: %u\n", md.rx_queues[e].qid);
        printf("\t\tring size: %u\n", md.rx_queues[e].ring_size);
        printf("\t\tbuffer size: %u\n", md.rx_queues[e].buffer_size);
    }
    printf("\ttx queues:\n");
    for (e = 0; e < md.tx_queues_num; e++) {
        printf("\t\tqueue id: %u\n", md.tx_queues[e].qid);
        printf("\t\tring size: %u\n", md.tx_queues[e].ring_size);
        printf("\t\tbuffer size: %u\n", md.tx_queues[e].buffer_size);
    }
    printf("\tlink: ");
    if (md.link_up_down)
        printf("up\n");
    else
        printf("down\n");

    free(buf);
}

/* informs user about connected status. private_ctx is used by user to identify
 * connection */
int on_connect(memif_conn_handle_t conn, void* priv_data)
{
    int err = 0;
    memif_conn_context* pmemif = (memif_conn_context*)priv_data;

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
//TBD        mesh_log(mc, MESH_LOG_ERROR, "memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    pmemif->is_connected = 1;

//TBD    mesh_log(mc, MESH_LOG_INFO, "memif connected!");
    return 0;
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    int err = 0;
    memif_conn_context* pmemif = (memif_conn_context*)priv_data;

    // if (pmemif->is_connected == 0)
    //     return 0;

    // INFO("Free memory");
    // free_memif_buffers(pmemif);

    /* stop event polling thread */
    err = memif_cancel_poll_event(memif_get_socket_handle(conn));
    if (err != MEMIF_ERR_SUCCESS) {
//TBD        mesh_log(mc, MESH_LOG_ERROR, "We are doomed...");
    }

    pmemif->is_connected = 0;

//TBD    mesh_log(mc, MESH_LOG_INFO, "memif disconnected!");
    return 0;
}

int tx_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    uint16_t rx_buf_num = 0;
    // memif_conn_context* pmemif = (memif_conn_context*)priv_data;
    memif_buffer_t rx_bufs = {};

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &rx_bufs, 1, &rx_buf_num);
    if (err != MEMIF_ERR_SUCCESS) {
//TBD        mesh_log(mc, MESH_LOG_ERROR, "memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    /* Do nothing, supposed this callback will never be called on TX side. */

    err = memif_refill_queue(conn, qid, rx_buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS) {
//TBD        mesh_log(mc, MESH_LOG_ERROR, "memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    return 0;
}

int rx_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    // uint16_t rx_buf_num = 0;
    memif_conn_context* pmemif = (memif_conn_context*)priv_data;
    // static int counter = 0;
    // static memif_buffer_t rx_buf = {};

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, pmemif->working_bufs, MEMIF_BUFFER_NUM, (uint16_t*)&pmemif->buf_num);
    if (err != MEMIF_ERR_SUCCESS) {
//TBD        mesh_log(mc, MESH_LOG_ERROR, "memif_rx_burst: %s", memif_strerror(err));
//TBD        mesh_log(mc, MESH_LOG_ERROR, "received buffer number: %d", pmemif->buf_num);
        // mesh_log(mc, MESH_LOG_ERROR, "buffer flag: %d, len: %d, index: %d\n",
        //     pmemif->working_bufs.flags, pmemif->working_bufs.len, pmemif->working_bufs.desc_index);
        // pmemif->buf_num = 0;
        return err;
    }

    pmemif->working_idx = 0;

    // pmemif->buf_num = rx_buf_num;

    /* save the received buffer. */
    // memcpy(&pmemif->working_bufs, &rx_buf, sizeof(memif_buffer_t));

    // err = memif_refill_queue(conn, qid, rx_buf_num, 0);
    // if (err != MEMIF_ERR_SUCCESS) {
    //     mesh_log(mc, MESH_LOG_ERROR, "memif_refill_queue: %s", memif_strerror(err));
    //     return err;
    // }

    // counter += rx_buf_num;
    // printf("RX received frames: %u\n", counter);

    return 0;
}

int mcm_create_connection_memif(MeshClient mc, MeshConnection conn, mcm_conn_param* svc_args, memif_conn_param* memif_args)
{
    int ret = 0;
    memif_conn_context* shm_conn = NULL;
    memif_socket_handle_t memif_socket;
    
    MeshConnectionConfig *conn_conf= (MeshConnectionConfig*) conn;    

    if (svc_args == NULL || memif_args == NULL) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal svc_argseters.");
        return MESH_CANNOT_CREATE_MEMIF_CONNECTION;
    }

    /* unlink socket file */
    if (memif_args->conn_args.is_master && memif_args->socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            if (mkdir("/run/mcm", 0666) == -1) {
                perror("Fail to create directory for memif.");
                return MESH_CANNOT_CREATE_MEMIF_CONNECTION;
            }
        }
        unlink(memif_args->socket_args.path);
    }

    mesh_log(mc, MESH_LOG_INFO, "Create memif socket.");
    ret = memif_create_socket(&memif_socket, &memif_args->socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        mesh_log(mc, MESH_LOG_INFO, "memif_create_socket: %s", memif_strerror(ret));
        return MESH_CANNOT_CREATE_MEMIF_CONNECTION;
    }

    /* Fill information about memif connection */
    shm_conn = calloc(1, sizeof(memif_conn_context));
    if (shm_conn == NULL) {
        mesh_log(mc, MESH_LOG_ERROR, "Out of Memory.");
        exit(-1);
    }
    shm_conn->sockfd = memif_socket;
    memif_args->conn_args.socket = memif_socket;

    mesh_log(mc, MESH_LOG_INFO, "Create memif interface.");
    if (svc_args->type == is_tx) {
        ret = memif_create(&shm_conn->conn, &memif_args->conn_args,
            on_connect, on_disconnect, tx_on_receive, shm_conn);
    } else {
        ret = memif_create(&shm_conn->conn, &memif_args->conn_args,
            on_connect, on_disconnect, rx_on_receive, shm_conn);
    }
    if (ret != MEMIF_ERR_SUCCESS) {
        mesh_log(mc, MESH_LOG_INFO, "memif_create: %s", memif_strerror(ret));
        free(shm_conn);
        shm_conn = NULL;
        memif_delete_socket(&memif_socket);
        return MESH_CANNOT_CREATE_MEMIF_CONNECTION;
    }

    do {
        ret = memif_poll_event(shm_conn->sockfd, -1);
        if (ret != MEMIF_ERR_SUCCESS) {
            mesh_log(mc, MESH_LOG_ERROR, "Create memif connection failed.");
            free(shm_conn);
            shm_conn = NULL;
            memif_delete_socket(&memif_socket);
            return MESH_CANNOT_CREATE_MEMIF_CONNECTION;
        }
    } while (shm_conn->is_connected == 0);

    if (conn == NULL) {
        mesh_log(mc, MESH_LOG_ERROR, "No connection.");
        free(shm_conn);
        shm_conn = NULL;
        memif_delete_socket(&memif_socket);
        return MESH_CANNOT_CREATE_MEMIF_CONNECTION;
    }

    shm_conn->buffer_size = memif_args->conn_args.buffer_size;
    /* connection type */
    if (svc_args->type == is_tx) {
        conn_conf->type = is_tx;
    } else {
        conn_conf->type = is_rx;
    }

    /* connection protocol */
    conn_conf->proto = PROTO_MEMIF;
    conn_conf->priv = (void*)shm_conn;
    /* video frame format */
    conn_conf->width = svc_args->width;
    conn_conf->height = svc_args->height;
    conn_conf->pix_fmt = svc_args->pix_fmt;
    conn_conf->fps = svc_args->fps;
    /* frame buffer size */
    conn_conf->frame_size = memif_args->conn_args.buffer_size;

    return MCM_DP_SUCCESS;
}

 int memif_dequeue_buffer(MeshClient mc, MeshConnection conn, mcm_buffer *buf, int timeout)
{
    int err = 0;
    memif_conn_context* memif_conn = NULL;
    memif_buffer_t memif_buf = {};
    uint16_t buf_num = 0;
    buf = NULL;
    MeshConnectionConfig *conn_conf= (MeshConnectionConfig*) conn;  

    if (!conn || !conn_conf->priv) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal Parameter.");
        return MCM_DP_ERROR_INVALID_PARAM;
    }
    memif_conn = (memif_conn_context*)conn_conf->priv;

    while (memif_conn->is_connected == 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Data connection stopped.");
        return MCM_DP_ERROR_UNKNOWN;
    }

    if (conn_conf->type == is_tx) {  /* TX */
        /* trigger the callbacks. */
        err = memif_poll_event(memif_conn->sockfd, 0);
        if (err != MEMIF_ERR_SUCCESS) {
            mesh_log(mc, MESH_LOG_INFO, "TX memif_poll_event: %s", memif_strerror(err));
            return MCM_DP_ERROR_UNKNOWN;
        }

        do {
            const size_t sleep_interval = 10; /* 0.01 s */
            err = memif_buffer_alloc(memif_conn->conn, memif_conn->qid, &memif_buf, 1,
                &buf_num, conn_conf->frame_size);
            if (err == MEMIF_ERR_SUCCESS) {
                break;
            } else {
                if (err == MEMIF_ERR_NOBUF_RING) {
                    // mesh_log(mc, MESH_LOG_ERROR, "Empty of memif buffer ring.");
                    if (timeout == 0) {
                        /* no wait */
                        break;
                    } else if (timeout < 0) {
                        continue;
                    } else {
                        /* trigger the callbacks. */
                        err = memif_poll_event(memif_conn->sockfd, sleep_interval);
                        if (err != MEMIF_ERR_SUCCESS) {
                            mesh_log(mc, MESH_LOG_INFO, "TX memif event: %s", memif_strerror(err));
                            break;
                        }
                        timeout -= sleep_interval;
                        timeout = timeout < 0 ? 0 : timeout;
                    }
                } else {
                    mesh_log(mc, MESH_LOG_ERROR, "Failed to alloc memif buffer: %s", memif_strerror(err));
                    break;
                }
            }
        } while (1);

        if (err == MEMIF_ERR_SUCCESS) {
            buf = calloc(1, sizeof(mcm_buffer));
            buf->len = conn_conf->frame_size;
            buf->data = memif_buf.data;
            memif_conn->working_bufs[0] = memif_buf;
            memif_conn->working_idx = 0;
            memif_conn->buf_num = buf_num;
        } else {
            mesh_log(mc, MESH_LOG_ERROR, "Failed to alloc buffer from memory queue.");
        }
    } else {    /* RX */
        /* waiting for the buffer ready from rx_on_receive callback. */
        if (memif_conn->buf_num <= 0) {
            err = memif_poll_event(memif_conn->sockfd, timeout);
        }

        if (err != MEMIF_ERR_SUCCESS) {
            mesh_log(mc, MESH_LOG_ERROR, "memif_poll_event: %s", memif_strerror(err));
        } else {
            if (memif_conn->buf_num > 0) {
                buf = calloc(1, sizeof(mcm_buffer));
                buf->len = memif_conn->working_bufs[memif_conn->working_idx].len;
                buf->data = memif_conn->working_bufs[memif_conn->working_idx].data;
                memif_conn->working_idx++;
                memif_conn->buf_num--;
            } else { /* Timeout */
                mesh_log(mc,MESH_LOG_DEBUG, "Timeout to read buffer from memory queue.");
                err=MCM_DP_ERROR_TIMEOUT;
            }
        }
     }

    return err;
}

int memif_enqueue_buffer(MeshClient mc, MeshConnection conn, mcm_buffer *buf, int timeout)
{
    int err = 0;
    memif_conn_context* memif_conn = NULL;
    uint16_t buf_num = 0;
    // static size_t frame_count = 0;
    MeshConnectionConfig *conn_conf= (MeshConnectionConfig*) conn;     

    if (!conn || !conn_conf->priv || !buf) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal Parameter.");
        return -1;
    }

    memif_conn = (memif_conn_context*)conn_conf->priv;

    if (memif_conn->is_connected == 0) {
        mesh_log(mc, MESH_LOG_ERROR, "Data connection stopped.");
        return -1;
    }

    if (conn_conf->type == is_tx) {
        if (buf->data != memif_conn->working_bufs[0].data) {
            mesh_log(mc, MESH_LOG_ERROR, "Unknown buffer address.");
            return -1;
        }

        /* set the actual size of data in the buffer */
        if (buf->len < memif_conn->working_bufs[0].len)
            memif_conn->working_bufs[0].len = buf->len;

        err = memif_tx_burst(memif_conn->conn, memif_conn->qid, &memif_conn->working_bufs[0], 1, &buf_num);
        if (err != MEMIF_ERR_SUCCESS) {
            mesh_log(mc, MESH_LOG_ERROR, "memif_tx_burst: %s", memif_strerror(err));
        }

        memif_conn->buf_num--;
        // frame_count++;
        // mesh_log(mc, MESH_LOG_INFO, "TX sent frames: %lu", frame_count);
    } else {
        err = memif_refill_queue(memif_conn->conn, memif_conn->qid, 1, 0);
        if (err != MEMIF_ERR_SUCCESS) {
            mesh_log(mc, MESH_LOG_ERROR, "memif_refill_queue: %s", memif_strerror(err));
        }
    }

    free(buf);

    return err;
}

void mcm_destroy_connection_memif(MeshClient mc, memif_conn_context* pctx)
{
    if (!pctx) {
        mesh_log(mc, MESH_LOG_ERROR, "Illegal Parameter.");
        return;
    }

    /* free-up resources */
    memif_delete(&pctx->conn);
    memif_delete_socket(&pctx->sockfd);

    free(pctx);

    return;
}

// /* Alloc buffer from queue. */
// void* memif_alloc_buffer(void* conn_ctx, size_t size)
// {
//     return NULL;
// }

// /* Send out video frame on TX side. */
// int memif_send_buffer(void* conn_ctx, mcm_buffer* buf)
// {
//     return 0;
// }

// /* Receive video frame on RX side. */
// int memif_recv_buffer(void* conn_ctx, mcm_buffer* buf)
// {
//     return 0;
// }

// /* Return video frame buffer to queue. */
// void memif_free_buffer(void* conn_ctx, mcm_buffer* buf)
// {
//     return;
// }
