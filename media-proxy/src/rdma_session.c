#include <pthread.h>
#include <sys/stat.h>

#include "libfabric_dev.h"
#include "rdma_session.h"


static void* rx_memif_event_loop(void* arg)
{
    int err = MEMIF_ERR_SUCCESS;
    memif_socket_handle_t memif_socket = (memif_socket_handle_t)arg;

    do {
        // INFO("media-proxy waiting event.");
        err = memif_poll_event(memif_socket, -1);
        // INFO("media-proxy received event.");
    } while (err == MEMIF_ERR_SUCCESS);

    INFO("MEMIF DISCONNECTED.");

    return NULL;
}

int rx_rdma_shm_init(rx_rdma_session_context_t* rx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    memif_ops_t default_memif_ops = { 0 };

    if (rx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(rx_ctx->memif_socket_args.app_name, sizeof(rx_ctx->memif_socket_args.app_name));
    bzero(rx_ctx->memif_socket_args.path, sizeof(rx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_rx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_rx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_rx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(rx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(rx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(rx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(rx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(rx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&rx_ctx->memif_socket, &rx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    rx_ctx->shm_ready = 0;
    rx_ctx->memif_conn_args.socket = rx_ctx->memif_socket;
    rx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    rx_ctx->memif_conn_args.buffer_size = (uint32_t)rx_ctx->transfer_size;
    rx_ctx->memif_conn_args.log2_ring_size = 2;
    memcpy((char*)rx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(rx_ctx->memif_conn_args.interface_name));
    rx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    INFO("create memif interface.");
    ret = memif_create(&rx_ctx->memif_conn, &rx_ctx->memif_conn_args,
        rx_rdma_on_connect, rx_rdma_on_disconnect, rx_on_receive, rx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&rx_ctx->memif_event_thread, NULL, rx_memif_event_loop, rx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        return -1;
    }

    return 0;
}

int tx_rdma_shm_init(tx_rdma_session_context_t* tx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    const uint16_t FRAME_COUNT = 4;
    memif_ops_t default_memif_ops = { 0 };

    if (tx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(tx_ctx->memif_socket_args.app_name, sizeof(tx_ctx->memif_socket_args.app_name));
    bzero(tx_ctx->memif_socket_args.path, sizeof(tx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_tx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_tx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_tx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(tx_ctx->memif_socket_args.app_name, memif_ops->app_name, sizeof(tx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(tx_ctx->memif_socket_args.path, memif_ops->socket_path, sizeof(tx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        struct stat st = { 0 };
        if (stat("/run/mcm", &st) == -1) {
            ret = mkdir("/run/mcm", 0666);
            if (ret != 0) {
                perror("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(tx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&tx_ctx->memif_socket, &tx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    tx_ctx->shm_ready = 0;
    tx_ctx->memif_conn_args.socket = tx_ctx->memif_socket;
    tx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    tx_ctx->memif_conn_args.buffer_size = (uint32_t)tx_ctx->transfer_size;
    tx_ctx->memif_conn_args.log2_ring_size = 2;
    snprintf((char*)tx_ctx->memif_conn_args.interface_name, sizeof(tx_ctx->memif_conn_args.interface_name), "%s", memif_ops->interface_name);
    tx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    /* TX buffers */
    tx_ctx->shm_bufs = (memif_buffer_t*)malloc(sizeof(memif_buffer_t) * FRAME_COUNT);
    tx_ctx->shm_buf_num = FRAME_COUNT;

    INFO("Create memif interface.");
    ret = memif_create(&tx_ctx->memif_conn, &tx_ctx->memif_conn_args, tx_rdma_on_connect, tx_rdma_on_disconnect, tx_rdma_on_receive, tx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        free(tx_ctx->shm_bufs);
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&tx_ctx->memif_event_thread, NULL, rx_memif_event_loop, tx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        free(tx_ctx->shm_bufs);
        return -1;
    }

    return 0;
}

static int rx_shm_deinit(rx_rdma_session_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(rx_ctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&rx_ctx->memif_conn);
    memif_delete_socket(&rx_ctx->memif_socket);

    /* unlink socket file */
    if (rx_ctx->memif_conn_args.is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        unlink(rx_ctx->memif_socket_args.path);
    }

    if (rx_ctx->shm_bufs) {
        free(rx_ctx->shm_bufs);
        rx_ctx->shm_bufs = NULL;
    }

    return 0;
}

static int tx_shm_deinit(tx_rdma_session_context_t* tx_ctx)
{
    if (tx_ctx == NULL) {
        printf("%s, Illegal parameter.\n", __func__);
        return -1;
    }

    pthread_cancel(tx_ctx->memif_event_thread);

    /* free-up resources */
    memif_delete(&tx_ctx->memif_conn);
    memif_delete_socket(&tx_ctx->memif_socket);

    /* unlink socket file */
    if (tx_ctx->memif_conn_args.is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        unlink(tx_ctx->memif_socket_args.path);
    }

    if (tx_ctx->shm_bufs) {
        free(tx_ctx->shm_bufs);
        tx_ctx->shm_bufs = NULL;
    }

    return 0;
}

/* TX: Create RDMA session */
tx_rdma_session_context_t* rdma_tx_session_create(libfabric_ctx* dev_handle, rdma_s_ops_t* opts, memif_ops_t* memif_ops){
    int ret = 0;
	tx_rdma_session_context_t* tx_ctx;
	tx_ctx = calloc(1, sizeof(tx_rdma_session_context_t));
    if (tx_ctx == NULL) {
        printf("%s, TX session contex malloc fail\n", __func__);
        return NULL;
    }
    tx_ctx->rdma_ctx = dev_handle;
    tx_ctx->stop = false;

    tx_ctx->transfer_size = opts->transfer_size;

    ret = tx_rdma_shm_init(tx_ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        free(tx_ctx);
        return NULL;
    }

    // TODO: use memif buffer with correct size
    ep_cfg_t ep_cfg = {
        .rdma_ctx = tx_ctx->rdma_ctx,
        .data_buf_size = tx_ctx->transfer_size,
        .local_addr = opts->local_addr,
        .remote_addr = opts->remote_addr,
        .data_buf = malloc(tx_ctx->transfer_size),
        .dir = opts->dir,
    };
    if (!ep_cfg.data_buf) {
        printf("%s, session data buffer malloc fail\n", __func__);
        return NULL;
    }

    ret = ep_init(&tx_ctx->ep_ctx, &ep_cfg);
    if (ret) {
        printf("%s, fail to initialize libfabric's end point.\n", __func__);
        free(tx_ctx);
        return NULL;
    }

    return tx_ctx;
}

static void rx_rdma_consume_frame(rx_rdma_session_context_t* s, char* frame)
{
    int err = 0;
    uint16_t qid = 0;
    mcm_buffer* rx_mcm_buff = NULL;
    memif_buffer_t* rx_bufs = NULL;
    uint16_t buf_num = 1;
    memif_conn_handle_t conn;
    uint32_t buf_size = s->transfer_size;
    uint16_t rx_buf_num = 0, rx = 0;

    if (s->shm_ready == 0) {
        INFO("%s memif not ready\n", __func__);
        return;
    }

    rx_bufs = s->shm_bufs;

    /* allocate memory */
    err = memif_buffer_alloc(s->memif_conn, qid, rx_bufs, buf_num, &rx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_rdma_consume_frame: Failed to alloc memif buffer: %s", memif_strerror(err));
        return;
    }

    memcpy(rx_bufs->data, frame, s->transfer_size);

    /* Send to microservice application. */
    err = memif_tx_burst(s->memif_conn, qid, rx_bufs, rx_buf_num, &rx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("rx_rdma_consume_frame memif_tx_burst: %s", memif_strerror(err));
    }

    s->fb_recv++;
}

static void* rx_rdma_frame_thread(void* arg)
{
    rx_rdma_session_context_t* s_ctx = (rx_rdma_session_context_t*)arg;
    libfabric_ctx* rdma_ctx = s_ctx->rdma_ctx;
    ep_ctx_t* cp_ctx = s_ctx->ep_ctx;

    printf("%s(%d), start\n", __func__, s_ctx->idx);
    while (!s_ctx->stop) {
        ep_recv_buf(cp_ctx, cp_ctx->data_buf ,s_ctx->transfer_size);
        rx_rdma_consume_frame(s_ctx, cp_ctx->data_buf);
    }

    return NULL;
}

/* RX: Create RDMA session */
rx_rdma_session_context_t* rdma_rx_session_create(libfabric_ctx* dev_handle, rdma_s_ops_t* opts, memif_ops_t* memif_ops){
    int ret = 0;
	rx_rdma_session_context_t* rx_ctx;
    rx_ctx = calloc(1, sizeof(rx_rdma_session_context_t));
    if (rx_ctx == NULL) {
        printf("%s, TX session contex malloc fail\n", __func__);
        return rx_ctx;
    }
    rx_ctx->rdma_ctx = dev_handle;
    rx_ctx->stop = false;

    rx_ctx->transfer_size = opts->transfer_size;

    ret = rx_rdma_shm_init(rx_ctx, memif_ops);

    if (ret < 0) {
        printf("%s, fail to initialize share memory.\n", __func__);
        free(rx_ctx);
        return NULL;
    }

    // TODO: use memif buffer with correct size
    ep_cfg_t ep_cfg = {
        .rdma_ctx = rx_ctx->rdma_ctx,
        .data_buf_size = rx_ctx->transfer_size,
        .local_addr = opts->local_addr,
        .remote_addr = opts->remote_addr,
        .data_buf = malloc(rx_ctx->transfer_size),
        .dir = opts->dir,
    };
    if (!ep_cfg.data_buf) {
        printf("%s, session data buffer malloc fail\n", __func__);
        return NULL;
    }
    ret = ep_init(&rx_ctx->ep_ctx, &ep_cfg);
    if (ret) {
        printf("%s, fail to initialize libfabric's end point.\n", __func__);
        free(rx_ctx);
        return NULL;
    }

    ret = pthread_create(&rx_ctx->frame_thread, NULL, rx_rdma_frame_thread, rx_ctx);
    if (ret < 0) {
        printf("%s(%d), thread create fail %d\n", __func__, ret, rx_ctx->idx);
        free(rx_ctx);
        return NULL;
    }

    return rx_ctx;
}

void rdma_rx_session_stop(rx_rdma_session_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }
    // TODO: Add better synchronization
    rx_ctx->stop = true;

    pthread_join(rx_ctx->frame_thread, NULL);
}

void rdma_rx_session_destroy(rx_rdma_session_context_t** p_rx_ctx)
{
    rx_rdma_session_context_t* rx_ctx = NULL;
    int ret;

    if (p_rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx = *p_rx_ctx;
    // TODO: Remove free when memif buf will be used
    free(rx_ctx->ep_ctx->data_buf);
    ret = ep_destroy(&rx_ctx->ep_ctx);
    if (ret < 0) {
        printf("%s, ep free failed\n", __func__);
        return;
    }

    rx_shm_deinit(rx_ctx);

    /* Free shared memory resource. */
    free(rx_ctx);
}

void rdma_tx_session_stop(tx_rdma_session_context_t* tx_ctx)
{
    if (tx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }
    if (!tx_ctx->shm_ready) {
        pthread_cancel(tx_ctx->memif_event_thread);
    }

    tx_ctx->stop = true;

    pthread_join(tx_ctx->memif_event_thread, NULL);
}

void rdma_tx_session_destroy(tx_rdma_session_context_t** p_tx_ctx)
{
    tx_rdma_session_context_t* tx_ctx = NULL;
    int ret;

    if (p_tx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    tx_ctx = *p_tx_ctx;
    // TODO: Remove free when memif buf will be used
    free(tx_ctx->ep_ctx->data_buf);
    ret = ep_destroy(&tx_ctx->ep_ctx);
    if (ret < 0) {
        printf("%s, ep free failed\n", __func__);
        return;
    }

    tx_shm_deinit(tx_ctx);

    /* Free shared memory resource. */
    free(tx_ctx);
}
