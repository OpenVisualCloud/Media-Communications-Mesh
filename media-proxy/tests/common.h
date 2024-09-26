/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h> // printf
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // unlink, sleep

#include "libmemif.h"
#include "shm_memif.h"
#include "utils.h"

#define FRAME_SIZE 8294400 // yuv422p10le (1920*1080*4)
#define FRAME_COUNT 1

/* maximum tx/rx memif buffers */
#define MAX_MEMIF_BUFS 256

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
        INFO("%s", memif_strerror(err));
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

void alloc_memif_buffers(shm_connection_t* c)
{
    c->rx_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * MAX_MEMIF_BUFS);
    c->rx_buf_num = 0;
    c->tx_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * MAX_MEMIF_BUFS);
    c->tx_buf_num = 0;
}

void free_memif_buffers(shm_connection_t* c)
{
    if (c->rx_bufs != NULL)
        free(c->rx_bufs);
    c->rx_bufs = NULL;
    c->rx_buf_num = 0;
    if (c->tx_bufs != NULL)
        free(c->tx_bufs);
    c->tx_bufs = NULL;
    c->tx_buf_num = 0;
}
