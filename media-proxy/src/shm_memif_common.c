/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>

#include "mtl.h"
#include "shm_memif.h"

void print_memif_details(memif_conn_handle_t conn)
{
    printf("MEMIF DETAILS\n");
    printf("==============================\n");

    memif_details_t md;
    memset(&md, 0, sizeof(md));
    ssize_t buflen = 2048;
    char* buf = NULL;
    int err = 0;

    buf = (char*)malloc(buflen);
    if (buf == NULL) {
        INFO("Not Enough Memory.");
        return;
    }
    memset(buf, 0, buflen);

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
    for (err = 0; err < md.rx_queues_num; err++) {
        printf("\t\tqueue id: %u\n", md.rx_queues[err].qid);
        printf("\t\tring size: %u\n", md.rx_queues[err].ring_size);
        printf("\t\tbuffer size: %u\n", md.rx_queues[err].buffer_size);
    }
    printf("\ttx queues:\n");
    for (err = 0; err < md.tx_queues_num; err++) {
        printf("\t\tqueue id: %u\n", md.tx_queues[err].qid);
        printf("\t\tring size: %u\n", md.tx_queues[err].ring_size);
        printf("\t\tbuffer size: %u\n", md.tx_queues[err].buffer_size);
    }
    printf("\tlink: ");
    if (md.link_up_down)
        printf("up\n");
    else
        printf("down\n");

    free(buf);
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int rx_on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    int err = 0;
    rx_session_context_t* rx_ctx = priv_data;
    memif_socket_handle_t socket;

    if (conn == NULL) {
        return 0;
    }

    // if (rx_ctx == NULL) {
    //     INFO("Invalid Parameters.");
    //     return -1;
    // }

    // release session
    // if (rx_ctx->shm_ready == 0) {
    //     return 0;
    // }
    // rx_ctx->shm_ready = 0;

    // mtl_st20p_rx_session_destroy(&rx_ctx);

    /* stop event polling thread */
    INFO("RX Stop poll event\n");
    socket = memif_get_socket_handle(conn);
    if (socket == NULL) {
        INFO("Invalide socket handle.");
        return -1;
    }

    err = memif_cancel_poll_event(socket);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("We are doomed...");
    }

    // free(priv_data);

    return 0;
}

int rx_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    memif_buffer_t shm_bufs;
    uint16_t buf_num = 0;

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    /* Process on the received buffer. */
    /* Supposed this function will never be called. */

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    return 0;
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int tx_on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    static int counter = 0;
    int err = 0;
    tx_session_context_t* tx_ctx = priv_data;
    memif_socket_handle_t socket;

    // if (tx_ctx == NULL) {
    //     INFO("Invalid Parameters.");
    //     return -1;
    // }

    // // release session
    // if (tx_ctx->shm_ready == 0) {
    //     return 0;
    // }
    // tx_ctx->shm_ready = 0;

    // mtl_st20p_tx_session_destroy(&tx_ctx);

    /* stop event polling thread */
    INFO("TX Stop poll event");
    socket = memif_get_socket_handle(conn);
    if (socket == NULL) {
        INFO("Invalide socket handle.");
        return -1;
    }

    err = memif_cancel_poll_event(socket);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("We are doomed...");
    }

    // free(priv_data);

    return 0;
}

int memif_buffer_alloc_timeout(memif_conn_handle_t conn, uint16_t qid,
                               memif_buffer_t * bufs, uint16_t count, uint16_t * count_out,
                               uint32_t size, uint32_t timeout_ms) {
    uint32_t waited_half_ms = 0;
    int err;

    while (waited_half_ms < timeout_ms * 2) {
        err = memif_buffer_alloc(conn, qid, bufs, count, count_out, size);
        if (err == MEMIF_ERR_NOBUF_RING) {
            usleep(500);
            waited_half_ms += 1;
            continue;
        }
        if (err)
            return err;
        else
            return 0;
    }
    return MEMIF_ERR_NOBUF_RING;
}

int memif_get_buffs_region(memif_conn_handle_t conn, memif_region_details_t *region)
{
    memif_details_t md = { 0 };
    ssize_t buflen = 2000;
    char* buf = NULL;
    int err = 0;

    if(!region || ! conn)
        return -EINVAL;

    buf = (char*)calloc(buflen, 1);
    if (buf == NULL) {
        ERROR("Not Enough Memory.");
        return -ENOMEM;
    }

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        ERROR("%s", memif_strerror(err));
        free(buf);
        return -EINVAL;
    }
    /* Region number 1 holds data buffers */
    if (md.regions_num < 1){
        ERROR("Data buffers not found in memif regions");
        free(buf);
        return -EINVAL;
    }

    memcpy(region, &md.regions[1], sizeof(md.regions[1]));
    free(buf);

    return 0;
}
