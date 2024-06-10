/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

#define APP_NAME "service-app-rx"
#define SOCKET_PATH "/run/mcm/media-proxy-rx-shm.sock"

/* for libmemif */
#define IF_NAME "rx-app-proxy-shm"
#define IF_ID 0

typedef struct {
    char* video_fn;
    FILE* video_fd;

    size_t frame_idx;
    uint16_t vid_width;
    uint16_t vid_height;
    size_t frame_size;
    uint32_t frame_cnt;

    shm_connection_t memif_intf;
    char* memif_app_name;
    char* memif_if_name;
    uint32_t memif_if_id;
    char* memif_socket_path;
} app_context_t;

/* informs user about connected status. private_ctx is used by user to identify
 * connection */
int on_connect(memif_conn_handle_t conn, void* priv_data)
{
    int err = 0;
    app_context_t* app_ctx = (app_context_t*)priv_data;
    shm_connection_t* pmemif = &app_ctx->memif_intf;

    INFO("RX memif connected!");

    alloc_memif_buffers(pmemif);

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    app_ctx->video_fd = fopen(app_ctx->video_fn, "wb+");
    if (app_ctx->video_fd == NULL) {
        perror("Fail to open output file.");
    }

    pmemif->is_connected = 1;

    return 0;
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    app_context_t* app_ctx = (app_context_t*)priv_data;
    shm_connection_t* pmemif = &app_ctx->memif_intf;

    if (pmemif->is_connected == 0)
        return 0;

    INFO("RX memif disconnected!");

    INFO("Free memory");
    free_memif_buffers(pmemif);

    /* stop event polling thread */
    INFO("RX stop poll event");
    int err = memif_cancel_poll_event(memif_get_socket_handle(conn));
    if (err != MEMIF_ERR_SUCCESS)
        INFO("We are doomed...");

    /* close output file. */
    if (app_ctx->video_fd)
        fclose(app_ctx->video_fd);

    pmemif->is_connected = 0;

    return 0;
}

int on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int ret = 0;
    uint16_t buf_num = FRAME_COUNT;
    uint16_t rx_buf_num = 0;
    app_context_t* app_ctx = (app_context_t*)priv_data;
    shm_connection_t* pmemif = &app_ctx->memif_intf;
    memif_buffer_t* rx_bufs = NULL;
    static int counter = 0;
    FILE* fp = NULL;

    rx_bufs = pmemif->rx_bufs;

    /* receive packets from the shared memory */
    ret = memif_rx_burst(conn, qid, rx_bufs, MAX_MEMIF_BUFS, &rx_buf_num);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_rx_burst: %s", memif_strerror(ret));
        return ret;
    }
    // INFO("RX received: size: %d, num: %d", rx_bufs->len, rx_buf_num);

    /* output to file */
    for (int i = 0; i < rx_buf_num; i++) {
        fwrite(rx_bufs[i].data, sizeof(uint8_t), rx_bufs[i].len, app_ctx->video_fd);
    }

    ret = memif_refill_queue(conn, qid, rx_buf_num, 0);
    if (ret != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(ret));

    counter += rx_buf_num;

    printf("RX[%u] received frames: %u\n", app_ctx->memif_if_id, app_ctx->frame_cnt++);

    return ret;
}

int main(int argc, char** argv)
{
    int ret = 0;
    int opt = 0;
    app_context_t app_ctx = { 0 };

    uint8_t is_master = 0;
    memif_socket_args_t memif_socket_args = { 0 };
    memif_socket_handle_t memif_socket;
    memif_conn_args_t memif_conn_args = { 0 };
    /* memif conenction handle */
    memif_conn_handle_t memif_conn = NULL;
    /*
    app_context_t app_ctx_1 = { 0 };
    memif_conn_handle_t memif_conn_1 = NULL;
    */

    app_ctx.video_fn = "./output0.yuv";
    app_ctx.memif_app_name = APP_NAME;
    app_ctx.memif_if_name = IF_NAME;
    app_ctx.memif_socket_path = SOCKET_PATH;

    while ((opt = getopt(argc, argv, "n:i:f:s:m")) != -1) {
        switch (opt) {
        case 'n':
            app_ctx.memif_app_name = optarg;
            break;
        case 'i':
            app_ctx.memif_if_name = optarg;
            break;
        case 'f':
            app_ctx.video_fn = optarg;
            break;
        case 's':
            app_ctx.memif_socket_path = optarg;
            break;
        case 'm':
            is_master = 1;
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-a App name] [-i interface name] [-f file] [-s socket] [-m] \n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    printf("MemIF Mode  : %s\n", is_master ? "Master" : "Slave");
    printf("MemIF App Name: %s\n", app_ctx.memif_app_name);
    printf("MemIF Interface Name: %s\n", app_ctx.memif_if_name);
    printf("MemIF Interface ID: %u\n", app_ctx.memif_if_id);
    printf("MemIF Socket: %s\n", app_ctx.memif_socket_path);
    printf("Output File : %s\n", app_ctx.video_fn);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(memif_socket_args.path, app_ctx.memif_socket_path, sizeof(memif_socket_args.path));

    /* Set application name */
    strncpy(memif_socket_args.app_name, app_ctx.memif_app_name, strlen(APP_NAME));

    /* unlink socket file */
    if (is_master && app_ctx.memif_socket_path[0] != '@') {
        unlink(app_ctx.memif_socket_path);
    }

    INFO("create memif socket.");
    ret = memif_create_socket(&memif_socket, &memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        exit(-1);
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    app_ctx.memif_if_id = IF_ID;
    memif_conn_args.socket = memif_socket;
    memif_conn_args.interface_id = app_ctx.memif_if_id;
    memif_conn_args.buffer_size = FRAME_SIZE;
    memif_conn_args.log2_ring_size = 2;
    strncpy(memif_conn_args.interface_name, app_ctx.memif_if_name,
        sizeof(memif_conn_args.interface_name));
    memif_conn_args.is_master = is_master;

    /* rx buffers */
    // memif_buffer_t *rx_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * FRAME_COUNT);

    INFO("Create memif interface.");
    ret = memif_create(&memif_conn, &memif_conn_args,
        on_connect, on_disconnect, on_receive, &app_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        exit(-1);
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    /*
    memcpy(&app_ctx_1, &app_ctx, sizeof(app_context_t));
    app_ctx_1.video_fn = "./output1.yuv";
    app_ctx_1.memif_if_id = IF_ID + 1;
    memif_conn_args.interface_id = app_ctx_1.memif_if_id;

    INFO("Create memif interface.");
    ret = memif_create(&memif_conn_1, &memif_conn_args,
        on_connect, on_disconnect, on_receive, &app_ctx_1);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        exit(-1);
    }
    */

    do {
        // INFO("RX waiting event.");
        ret = memif_poll_event(memif_socket, -1);
        // INFO("RX received event.");
    } while (ret == MEMIF_ERR_SUCCESS);

    /* free-up resources */
    memif_delete(&memif_conn);
    // memif_delete(&memif_conn_1);
    memif_delete_socket(&memif_socket);

    /* unlink socket file */
    if (is_master && app_ctx.memif_socket_path[0] != '@') {
        unlink(app_ctx.memif_socket_path);
    }

    return ret;
}
