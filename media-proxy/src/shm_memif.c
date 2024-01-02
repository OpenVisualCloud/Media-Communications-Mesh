/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
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

/* informs user about connected status. private_ctx is used by user to identify
 * connection */
int rx_st20p_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    rx_session_context_t* rx_ctx = (rx_session_context_t*)priv_data;
    int err = 0;

    INFO("RX memif connected!");

#if defined(ZERO_COPY)
    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    rx_ctx->fb_count = md.tx_queues[0].ring_size;
    rx_ctx->source_begin = md.regions[1].addr;
    rx_ctx->source_begin_iova_map_sz = md.regions[1].size;
    rx_ctx->source_begin_iova = mtl_dma_map(rx_ctx->st, rx_ctx->source_begin, md.regions[1].size);
    if (rx_ctx->source_begin_iova == MTL_BAD_IOVA) {
        ERROR("Fail to map DMA memory address.");
        return -1;
    }

    free(buf);
#else
    rx_ctx->fb_count = 3;
#endif

    /* rx buffers */
    rx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * rx_ctx->fb_count);
    rx_ctx->shm_buf_num = rx_ctx->fb_count;

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    rx_ctx->shm_ready = 1;

    return 0;
}

int rx_st22p_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    rx_st22p_session_context_t* rx_ctx = (rx_st22p_session_context_t*)priv_data;
    int err = 0;

    INFO("RX memif connected!");

#if defined(ZERO_COPY)
    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    rx_ctx->fb_count = md.rx_queues[0].ring_size;
    rx_ctx->source_begin = md.regions[1].addr;
    rx_ctx->source_begin_iova_map_sz = md.regions[1].size;
    rx_ctx->source_begin_iova = mtl_dma_map(rx_ctx->st, rx_ctx->source_begin, md.regions[1].size);
    if (rx_ctx->source_begin_iova == MTL_BAD_IOVA) {
        ERROR("Fail to map DMA memory address.");
        return -1;
    }

    free(buf);
#else
    rx_ctx->fb_count = 3;
#endif

    /* rx buffers */
    rx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * rx_ctx->fb_count);
    rx_ctx->shm_buf_num = rx_ctx->fb_count;

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    rx_ctx->shm_ready = 1;

    return 0;
}

int rx_st30_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    rx_st30_session_context_t* rx_ctx = (rx_st30_session_context_t*)priv_data;
    int err = 0;

    INFO("RX memif connected!");

    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    rx_ctx->fb_count = md.rx_queues[0].ring_size;

    free(buf);

    /* RX buffers */
    rx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * rx_ctx->fb_count);
    rx_ctx->shm_buf_num = rx_ctx->fb_count;

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    rx_ctx->shm_ready = 1;

    return 0;
}

int rx_st40_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    rx_st40_session_context_t* rx_ctx = (rx_st40_session_context_t*)priv_data;
    int err = 0;

    INFO("RX memif connected!");

    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    rx_ctx->fb_count = md.rx_queues[0].ring_size;

    free(buf);

    /* RX buffers */
    rx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * rx_ctx->fb_count);
    rx_ctx->shm_buf_num = rx_ctx->fb_count;

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    rx_ctx->shm_ready = 1;

    return 0;
}

int rx_udp_h264_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    rx_udp_h264_session_context_t* rx_ctx = (rx_udp_h264_session_context_t*)priv_data;
    int err = 0;

    INFO("RX memif connected!");

    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    rx_ctx->fb_count = md.rx_queues[0].ring_size;

    free(buf);

    /* rx buffers */
    rx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * rx_ctx->fb_count);
    rx_ctx->shm_buf_num = rx_ctx->fb_count;

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    print_memif_details(conn);

    rx_ctx->shm_ready = 1;

    return 0;
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

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    return 0;
}

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int rx_st20p_on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    int err = 0;
    rx_session_context_t* rx_ctx = priv_data;
    memif_socket_handle_t socket;

    if (conn == NULL) {
        return 0;
    }

    if (conn == NULL || priv_data == NULL) {
        INFO("Invalid Parameters.");
        return -1;
    }

    // release session
    if (rx_ctx->shm_ready == 0) {
        return 0;
    }
    rx_ctx->shm_ready = 0;

    // mtl_st20p_rx_session_destroy(&rx_ctx);

#if defined(ZERO_COPY)
    if (mtl_dma_unmap(rx_ctx->st, rx_ctx->source_begin, rx_ctx->source_begin_iova, rx_ctx->source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }
#endif

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

int rx_st22p_on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    int err = 0;
    rx_st22p_session_context_t* rx_ctx = priv_data;
    memif_socket_handle_t socket;

    if (conn == NULL || priv_data == NULL) {
        INFO("Invalid Parameters.");
        return -1;
    }

    // release session
    if (rx_ctx->shm_ready == 0) {
        return 0;
    }
    rx_ctx->shm_ready = 0;

#if defined(ZERO_COPY)
    if (mtl_dma_unmap(rx_ctx->st, rx_ctx->source_begin, rx_ctx->source_begin_iova, rx_ctx->source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }
#endif

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

    return 0;
}

int tx_st20p_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    tx_session_context_t* tx_ctx = (tx_session_context_t*)priv_data;
    int err = 0;

    INFO("TX memif connected!");

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

#if defined(ZERO_COPY)
    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    tx_ctx->source_begin = md.regions[1].addr;
    tx_ctx->source_begin_iova_map_sz = md.regions[1].size;
    tx_ctx->source_begin_iova = mtl_dma_map(tx_ctx->st, tx_ctx->source_begin, md.regions[1].size);
    if (tx_ctx->source_begin_iova == MTL_BAD_IOVA) {
        ERROR("Fail to map DMA memory address.");
        return -1;
    }

    free(buf);
#endif

    tx_ctx->shm_ready = 1;

    print_memif_details(conn);

    return 0;
}

int tx_st22p_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    tx_st22p_session_context_t* tx_ctx = (tx_st22p_session_context_t*)priv_data;
    int err = 0;

    INFO("TX memif connected!");

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

#if defined(ZERO_COPY)
    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    tx_ctx->source_begin = md.regions[1].addr;
    tx_ctx->source_begin_iova_map_sz = md.regions[1].size;
    tx_ctx->source_begin_iova = mtl_dma_map(tx_ctx->st, tx_ctx->source_begin, md.regions[1].size);
    if (tx_ctx->source_begin_iova == MTL_BAD_IOVA) {
        ERROR("Fail to map DMA memory address.");
        return -1;
    }

    free(buf);
#endif

    tx_ctx->shm_ready = 1;

    print_memif_details(conn);

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

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int tx_st20p_on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    static int counter = 0;
    int err = 0;
    tx_session_context_t* tx_ctx = priv_data;
    memif_socket_handle_t socket;

    if (conn == NULL || priv_data == NULL) {
        INFO("Invalid Parameters.");
        return -1;
    }

    // release session
    if (tx_ctx->shm_ready == 0) {
        return 0;
    }
    tx_ctx->shm_ready = 0;

    // mtl_st20p_tx_session_destroy(&tx_ctx);

#if defined(ZERO_COPY)
    if (mtl_dma_unmap(tx_ctx->st, tx_ctx->source_begin, tx_ctx->source_begin_iova, tx_ctx->source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }
#endif

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

/* informs user about disconnected status. private_ctx is used by user to
 * identify connection */
int tx_st22p_on_disconnect(memif_conn_handle_t conn, void* priv_data)
{
    static int counter = 0;
    int err = 0;
    tx_st22p_session_context_t* tx_ctx = priv_data;
    memif_socket_handle_t socket;

    if (conn == NULL || priv_data == NULL) {
        INFO("Invalid Parameters.");
        return -1;
    }

    // release session
    if (tx_ctx->shm_ready == 0) {
        return 0;
    }
    tx_ctx->shm_ready = 0;

    // mtl_st22p_tx_session_destroy(&tx_ctx);

#if defined(ZERO_COPY)
    if (mtl_dma_unmap(tx_ctx->st, tx_ctx->source_begin, tx_ctx->source_begin_iova, tx_ctx->source_begin_iova_map_sz) < 0) {
        ERROR("Fail to unmap DMA memory address.");
    }
#endif

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

static void tx_st20p_build_frame(memif_buffer_t shm_bufs, struct st_frame* frame)
{
    mtl_memcpy(frame->addr[0], shm_bufs.data, shm_bufs.len);
}

static void tx_st22p_build_frame(memif_buffer_t shm_bufs, struct st_frame* frame)
{
    mtl_memcpy(frame->addr[0], shm_bufs.data, shm_bufs.len);
}

static void tx_st30_build_frame(memif_buffer_t shm_bufs, void* frame, size_t frame_size)
{
    mtl_memcpy(frame, shm_bufs.data, frame_size);
}

static void tx_st40_build_frame(memif_buffer_t shm_bufs, struct st40_frame* dst)
{
    dst->meta[0].c = 0;
    dst->meta[0].line_number = 10;
    dst->meta[0].hori_offset = 0;
    dst->meta[0].s = 0;
    dst->meta[0].stream_num = 0;
    dst->meta[0].did = 0x43;
    dst->meta[0].sdid = 0x02;
    dst->meta[0].udw_size = shm_bufs.len;
    dst->meta[0].udw_offset = 0;
    dst->meta_num = 1;
    dst->data = shm_bufs.data;
    dst->data_size = shm_bufs.len;
}

int tx_st20p_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    tx_session_context_t* tx_ctx = (tx_session_context_t*)priv_data;
    memif_buffer_t shm_bufs = { 0 };
    uint16_t buf_num = 0;

    if (tx_ctx->stop) {
        INFO("TX session already stopped.");
        return -1;
    }

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }
    // INFO("memif_rx_burst: size: %d, num: %d", shm_bufs.len, buf_num);

    st20p_tx_handle handle = tx_ctx->handle;
    struct st_frame* frame = NULL;

    do {
        frame = st20p_tx_get_frame(handle);
        if (!frame) {
            st_pthread_mutex_lock(&tx_ctx->wake_mutex);
            if (!tx_ctx->stop)
                st_pthread_cond_wait(&tx_ctx->wake_cond, &tx_ctx->wake_mutex);
            st_pthread_mutex_unlock(&tx_ctx->wake_mutex);
        }
    } while (!frame);

    /* Send out frame. */
#if defined(ZERO_COPY)
    struct st_ext_frame ext_frame = { 0 };
    ext_frame.addr[0] = shm_bufs.data;
    ext_frame.iova[0] = tx_ctx->source_begin_iova + ((uint8_t*)shm_bufs.data - tx_ctx->source_begin);
    ext_frame.linesize[0] = st_frame_least_linesize(frame->fmt, frame->width, 0);
    uint8_t planes = st_frame_fmt_planes(frame->fmt);
    for (uint8_t plane = 1; plane < planes; plane++) { /* assume planes continous */
        ext_frame.linesize[plane] = st_frame_least_linesize(frame->fmt, frame->width, plane);
        ext_frame.addr[plane] = (uint8_t*)ext_frame.addr[plane - 1] + ext_frame.linesize[plane - 1] * frame->height;
        ext_frame.iova[plane] = ext_frame.iova[plane - 1] + ext_frame.linesize[plane - 1] * frame->height;
    }
    ext_frame.size = shm_bufs.len;
    ext_frame.opaque = conn;

    st20p_tx_put_ext_frame(handle, frame, &ext_frame);
#else
    /* fill frame data. */
    tx_st20p_build_frame(shm_bufs, frame);
    /* Send out frame. */
    st20p_tx_put_frame(handle, frame);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));
#endif

    // INFO("%s send frame %d", __func__, tx_ctx->fb_send);
    tx_ctx->fb_send++;

    return 0;
}

int tx_st22p_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    tx_st22p_session_context_t* tx_ctx = (tx_st22p_session_context_t*)priv_data;
    memif_buffer_t shm_bufs = { 0 };
    uint16_t buf_num = 0;

    if (tx_ctx->stop) {
        INFO("TX session already stopped.");
        return -1;
    }

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }
    // INFO("memif_rx_burst: size: %d, num: %d", shm_bufs.len, buf_num);

    st22p_tx_handle handle = tx_ctx->handle;
    struct st_frame* frame = NULL;

    do {
        frame = st22p_tx_get_frame(handle);
        if (!frame) {
            st_pthread_mutex_lock(&tx_ctx->st22p_wake_mutex);
            if (!tx_ctx->stop)
                st_pthread_cond_wait(&tx_ctx->st22p_wake_cond, &tx_ctx->st22p_wake_mutex);
            st_pthread_mutex_unlock(&tx_ctx->st22p_wake_mutex);
        }
    } while (!frame);

    /* Send out frame. */
#if defined(ZERO_COPY)
    struct st_ext_frame ext_frame = { 0 };
    ext_frame.addr[0] = shm_bufs.data;
    ext_frame.iova[0] = tx_ctx->source_begin_iova + ((uint8_t*)shm_bufs.data - tx_ctx->source_begin);
    ext_frame.linesize[0] = st_frame_least_linesize(frame->fmt, frame->width, 0);
    uint8_t planes = st_frame_fmt_planes(frame->fmt);
    for (uint8_t plane = 1; plane < planes; plane++) { /* assume planes continous */
        ext_frame.linesize[plane] = st_frame_least_linesize(frame->fmt, frame->width, plane);
        ext_frame.addr[plane] = (uint8_t*)ext_frame.addr[plane - 1] + ext_frame.linesize[plane - 1] * frame->height;
        ext_frame.iova[plane] = ext_frame.iova[plane - 1] + ext_frame.linesize[plane - 1] * frame->height;
    }
    ext_frame.size = shm_bufs.len;
    ext_frame.opaque = conn;

    st22p_tx_put_ext_frame(handle, frame, &ext_frame);
#else
    /* fill frame data. */
    tx_st22p_build_frame(shm_bufs, frame);
    /* Send out frame. */
    st22p_tx_put_frame(handle, frame);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));
#endif

    // INFO("%s send frame %d", __func__, tx_ctx->fb_send);
    tx_ctx->fb_send++;

    return 0;
}

int tx_st30_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    tx_st30_session_context_t* tx_ctx = (tx_st30_session_context_t*)priv_data;
    memif_buffer_t shm_bufs = { 0 };
    uint16_t buf_num = 0;

    uint16_t producer_idx;
    struct st_tx_frame* framebuff;

    if (tx_ctx->stop) {
        INFO("TX session already stopped.");
        return -1;
    }

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    while (1) {
        st_pthread_mutex_lock(&tx_ctx->st30_wake_mutex);
        producer_idx = tx_ctx->framebuff_producer_idx;
        framebuff = &tx_ctx->framebuffs[producer_idx];
        if (ST_TX_FRAME_FREE != framebuff->stat) {
            /* not in free */
            if (!tx_ctx->stop)
                st_pthread_cond_wait(&tx_ctx->st30_wake_cond, &tx_ctx->st30_wake_mutex);
            st_pthread_mutex_unlock(&tx_ctx->st30_wake_mutex);
            continue;
        }
        st_pthread_mutex_unlock(&tx_ctx->st30_wake_mutex);
        break;
    }

    void* frame_addr = st30_tx_get_framebuffer(tx_ctx->handle, producer_idx);

    /* fill frame data. */
    tx_st30_build_frame(shm_bufs, frame_addr, tx_ctx->st30_frame_size);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    st_pthread_mutex_lock(&tx_ctx->st30_wake_mutex);
    framebuff->size = tx_ctx->st30_frame_size;
    framebuff->stat = ST_TX_FRAME_READY;
    /* point to next */
    producer_idx++;
    if (producer_idx >= tx_ctx->framebuff_cnt)
        producer_idx = 0;
    tx_ctx->framebuff_producer_idx = producer_idx;
    st_pthread_mutex_unlock(&tx_ctx->st30_wake_mutex);

    return 0;
}

int tx_st30_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    tx_st30_session_context_t* tx_ctx = (tx_st30_session_context_t*)priv_data;
    int err = 0;

    INFO("TX memif connected!");

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    tx_ctx->fb_count = md.tx_queues[0].ring_size;

    free(buf);

    /* TX buffers */
    tx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * tx_ctx->fb_count);
    tx_ctx->shm_buf_num = tx_ctx->fb_count;

    tx_ctx->shm_ready = 1;

    print_memif_details(conn);

    return 0;
}

int tx_st40_on_connect(memif_conn_handle_t conn, void* priv_data)
{
    tx_st40_session_context_t* tx_ctx = (tx_st40_session_context_t*)priv_data;
    int err = 0;

    INFO("TX memif connected!");

    err = memif_refill_queue(conn, 0, -1, 0);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_refill_queue: %s", memif_strerror(err));
        return err;
    }

    memif_details_t md = { 0 };
    ssize_t buflen = 2048;
    char* buf = (char*)calloc(1, buflen);

    err = memif_get_details(conn, &md, buf, buflen);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("%s", memif_strerror(err));
        free(buf);
        return err;
    }

    tx_ctx->fb_count = md.tx_queues[0].ring_size;

    free(buf);

    tx_ctx->shm_ready = 1;

    print_memif_details(conn);

    return 0;
}

int tx_st40_on_receive(memif_conn_handle_t conn, void* priv_data, uint16_t qid)
{
    int err = 0;
    tx_st40_session_context_t* tx_ctx = (tx_st40_session_context_t*)priv_data;
    memif_buffer_t shm_bufs = { 0 };
    uint16_t buf_num = 0;

    uint16_t producer_idx;
    struct st_tx_frame* framebuff;

    if (tx_ctx->stop) {
        INFO("TX session already stopped.");
        return -1;
    }

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    while (1) {
        st_pthread_mutex_lock(&tx_ctx->st40_wake_mutex);
        producer_idx = tx_ctx->framebuff_producer_idx;
        framebuff = &tx_ctx->framebuffs[producer_idx];
        if (ST_TX_FRAME_FREE != framebuff->stat) {
            /* not in free */
            if (!tx_ctx->stop)
                st_pthread_cond_wait(&tx_ctx->st40_wake_cond, &tx_ctx->st40_wake_mutex);
            st_pthread_mutex_unlock(&tx_ctx->st40_wake_mutex);
            continue;
        }
        st_pthread_mutex_unlock(&tx_ctx->st40_wake_mutex);
        break;
    }

    struct st40_frame* frame_addr = st40_tx_get_framebuffer(tx_ctx->handle, producer_idx);
    // void* mbuf;
    // void* usrptr = NULL;
    // uint16_t mbuf_len = 0;
    // /* get available buffer */
    // mbuf = st40_tx_get_mbuf(tx_ctx->handle, &usrptr);
    // if (!mbuf) {
    //     return -1;
    // }

    /* fill frame data. */
    tx_st40_build_frame(shm_bufs, frame_addr);
    // tx_st40_build_packet(shm_bufs, usrptr, &mbuf_len);
    // st40_tx_put_mbuf(tx_ctx->handle, mbuf, mbuf_len);

    err = memif_refill_queue(conn, qid, buf_num, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));

    st_pthread_mutex_lock(&tx_ctx->st40_wake_mutex);
    framebuff->size = sizeof(*frame_addr);
    framebuff->stat = ST_TX_FRAME_READY;
    /* point to next */
    producer_idx++;
    if (producer_idx >= tx_ctx->framebuff_cnt)
        producer_idx = 0;
    tx_ctx->framebuff_producer_idx = producer_idx;
    st_pthread_mutex_unlock(&tx_ctx->st40_wake_mutex);

    return 0;
}
