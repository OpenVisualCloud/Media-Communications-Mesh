/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

#define APP_NAME "service-app-tx"
#define SOCKET_PATH "/run/mcm/media-proxy-tx-shm.sock"

/* for libmemif */
#define IF_NAME "tx-app-proxy-shm"
#define IF_ID 0

typedef struct {
    char* video_fn;
    FILE* video_fd;

    size_t frame_idx;
    uint16_t width;
    uint16_t height;
    size_t frame_size;

    bool loop_mode;
    shm_connection_t memif_intf;
    char* memif_app_name;
    char* memif_if_name;
    uint32_t memif_if_id;
    char* memif_socket_path;
} app_context_t;

int build_frames(app_context_t* p_app_ctx, memif_buffer_t* tx_bufs, uint32_t buf_size, uint16_t buf_num)
{
    int ret = 0;

    if (p_app_ctx->video_fd == NULL) {
        return -1;
    }

    for (int i = 0; i < buf_num; i++) {
        if (fread(tx_bufs[i].data, buf_size, buf_num, p_app_ctx->video_fd) < buf_num) {
            if (p_app_ctx->loop_mode) {
                rewind(p_app_ctx->video_fd);
                ret = 0;
            } else {
                perror("End of file. Read frame resulted in ");
                ret = -1;
            }
            break;
        }
    }

    return ret;
}

int try_send_msg(app_context_t* p_app_ctx)
{
    int err = 0;

    /* allocate memory */
    uint16_t qid = 0;
    uint32_t buf_size = p_app_ctx->frame_size;
    uint16_t tx_buf_num = 0, tx = 0;
    memif_buffer_t* tx_bufs = NULL;
    shm_connection_t* pmemif = &p_app_ctx->memif_intf;

    if (!pmemif->is_connected) {
        return 0;
    }

    tx_bufs = pmemif->tx_bufs;

    // INFO("Alloc memory for memif.");
    err = memif_buffer_alloc(pmemif->conn, pmemif->qid, pmemif->tx_bufs, 1,
        &tx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS) {
        if (err == MEMIF_ERR_NOBUF_RING) {
            usleep(1000); /* 1ms */
            return 0;
        } else {
            INFO("Failed to alloc memif buffer: %s", memif_strerror(err));
            return -1;
        }
    }

    /* Build frame buffer into shared memory. */
    err = build_frames(p_app_ctx, tx_bufs, buf_size, tx_buf_num);
    if (err != 0) {
        return -1;
    }

    err = memif_tx_burst(pmemif->conn, pmemif->qid, pmemif->tx_bufs, tx_buf_num, &tx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_tx_burst: %s", memif_strerror(err));
        return -1;
    }

    p_app_ctx->frame_idx += tx;

    printf("TX sent frames: %lu\n", p_app_ctx->frame_idx);

    return 0;
}

/* informs user about connected status. private_ctx is used by user to identify
 * connection */
int on_connect(memif_conn_handle_t conn, void* priv_data)
{
    int err = 0;
    shm_connection_t* pmemif = (shm_connection_t*)priv_data;

    INFO("TX memif connected!");

    alloc_memif_buffers(pmemif);

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    pmemif->is_connected = 1;

    return 0;
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    INFO("TX memif disconnected!");

    shm_connection_t* pmemif = (shm_connection_t*)priv_data;

    pmemif->is_connected = 0;

    INFO("Free memory");
    free_memif_buffers(pmemif);

    /* stop event polling thread */
    INFO("TX stop poll event");
    int err = memif_cancel_poll_event(memif_get_socket_handle(conn));
    if (err != MEMIF_ERR_SUCCESS)
        INFO("We are doomed...");

    return 0;
}

int on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    uint16_t rx_buf_num = 0;
    shm_connection_t* pmemif = (shm_connection_t*)priv_data;
    memif_buffer_t* rx_bufs = NULL;

    INFO("TX on receive.");

    rx_bufs = pmemif->rx_bufs;

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, rx_bufs, MAX_MEMIF_BUFS, &rx_buf_num);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    err = memif_refill_queue(conn, qid, rx_buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    return 0;
}

void* send_msg_thread(void* arg)
{
    int ret = 0;
    app_context_t* p_app_ctx = (app_context_t*)arg;

    p_app_ctx->video_fd = fopen(p_app_ctx->video_fn, "rb");
    if (p_app_ctx->video_fd == NULL) {
        perror("Fail to open video source file.");
        return NULL;
    }
    p_app_ctx->frame_idx = 0;

    do {
        usleep(16666); /* 1/60 (sec) */
        ret = try_send_msg(p_app_ctx);
    } while (ret == 0);

    fclose(p_app_ctx->video_fd);

    return NULL;
}

int main(int argc, char** argv)
{
    int ret = 0;
    int opt = 0;
    pthread_t frame_thread;
    app_context_t app_ctx = { 0 };

    uint8_t is_master = 0;
    memif_socket_args_t memif_socket_args = { 0 };
    memif_socket_handle_t memif_socket;
    memif_conn_args_t memif_conn_args = { 0 };

    app_ctx.loop_mode = false;
    app_ctx.video_fn = "./test.yuv";
    app_ctx.memif_app_name = APP_NAME;
    app_ctx.memif_if_name = IF_NAME;
    app_ctx.memif_if_id = IF_ID;
    app_ctx.memif_socket_path = SOCKET_PATH;
    app_ctx.width = 1920;
    app_ctx.height = 1080;

    while ((opt = getopt(argc, argv, "w:h:n:i:d:f:s:ml")) != -1) {
        switch (opt) {
        case 'w':
            app_ctx.width = atoi(optarg);
            break;
        case 'h':
            app_ctx.height = atoi(optarg);
            break;
        case 'n':
            app_ctx.memif_app_name = optarg;
            break;
        case 'i':
            app_ctx.memif_if_name = optarg;
            break;
        case 'd':
            app_ctx.memif_if_id = atoi(optarg);
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
        case 'l':
            app_ctx.loop_mode = true;
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-w width] [-h height] [-n app_name] [-i interface_name] [-d interface_id] [-f file] [-s socket] [-m] [-l] \n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    app_ctx.frame_size = (size_t)(app_ctx.width) * (size_t)(app_ctx.height) * 4;

    printf("Input File  : %s\n", app_ctx.video_fn);
    printf("MemIF Mode  : %s\n", is_master ? "Master" : "Slave");
    printf("MemIF App Name: %s\n", app_ctx.memif_app_name);
    printf("MemIF Interface Name: %s\n", app_ctx.memif_if_name);
    printf("MemIF Interface ID: %u\n", app_ctx.memif_if_id);
    printf("MemIF Socket: %s\n", app_ctx.memif_socket_path);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(memif_socket_args.path, app_ctx.memif_socket_path, sizeof(memif_socket_args.path));

    /* Set application name */
    strncpy(memif_socket_args.app_name, app_ctx.memif_app_name, sizeof(memif_socket_args.app_name));

    /* unlink socket file */
    if (is_master && app_ctx.memif_socket_path[0] != '@') {
        unlink(app_ctx.memif_socket_path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&memif_socket, &memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        exit(-1);
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    memif_conn_args.socket = memif_socket;
    memif_conn_args.buffer_size = app_ctx.frame_size;
    memif_conn_args.log2_ring_size = 2;
    strncpy(memif_conn_args.interface_name, app_ctx.memif_if_name,
        sizeof(memif_conn_args.interface_name));
    memif_conn_args.interface_id = app_ctx.memif_if_id;
    memif_conn_args.is_master = is_master;

    INFO("Create memif interface.");
    ret = memif_create(&app_ctx.memif_intf.conn, &memif_conn_args,
        on_connect, on_disconnect, on_receive, &app_ctx.memif_intf);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        exit(-1);
    }

    app_ctx.memif_intf.is_master = is_master;

    ret = pthread_create(&frame_thread, NULL, send_msg_thread, &app_ctx);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        exit(-1);
    }

    do {
        // INFO("TX waiting event.");
        ret = memif_poll_event(memif_socket, 0);
        // INFO("TX received event.");
        if (pthread_tryjoin_np(frame_thread, NULL) == 0) {
            INFO("Video producer terminated.\n");
            break;
        }
    } while (ret == MEMIF_ERR_SUCCESS);

    /* free-up resources */
    memif_delete(&app_ctx.memif_intf.conn);
    memif_delete_socket(&memif_socket);

    /* unlink socket file */
    if (is_master && app_ctx.memif_socket_path[0] != '@') {
        unlink(app_ctx.memif_socket_path);
    }

    return 0;
}
