#include <pthread.h>
#include <sys/stat.h>
#include <bsd/string.h>

#include "libfabric_dev.h"
#include "rdma_session.h"

static void *memif_event_loop(void *arg)
{
    memif_socket_handle_t memif_socket = (memif_socket_handle_t)arg;
    int err;

    do {
        // INFO("media-proxy waiting event.");
        err = memif_poll_event(memif_socket, -1);
        // INFO("media-proxy received event.");
    } while (err == MEMIF_ERR_SUCCESS);

    INFO("MEMIF DISCONNECTED.");

    return NULL;
}

int rx_rdma_shm_init(rx_rdma_session_context_t *rx_ctx, memif_ops_t *memif_ops)
{
    memif_ops_t default_memif_ops = { 0 };
    struct stat st = { 0 };
    int err;

    if (rx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(rx_ctx->memif_socket_args.app_name, sizeof(rx_ctx->memif_socket_args.app_name));
    bzero(rx_ctx->memif_socket_args.path, sizeof(rx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strlcpy(default_memif_ops.app_name, "mcm_rx", sizeof(default_memif_ops.app_name));
        strlcpy(default_memif_ops.interface_name, "mcm_rx",
                sizeof(default_memif_ops.interface_name));
        strlcpy(default_memif_ops.socket_path, "/run/mcm/mcm_rx_memif.sock",
                sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strlcpy(rx_ctx->memif_socket_args.app_name, memif_ops->app_name,
            sizeof(rx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strlcpy(rx_ctx->memif_socket_args.path, memif_ops->socket_path,
            sizeof(rx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        if (stat("/run/mcm", &st) == -1) {
            err = mkdir("/run/mcm", 0666);
            if (err != 0) {
                ERROR("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(rx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    err = memif_create_socket(&rx_ctx->memif_socket, &rx_ctx->memif_socket_args, NULL);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(err));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    rx_ctx->shm_ready = ATOMIC_VAR_INIT(false);
    rx_ctx->memif_conn_args.socket = rx_ctx->memif_socket;
    rx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    rx_ctx->memif_conn_args.buffer_size = (uint32_t)rx_ctx->transfer_size;
    rx_ctx->memif_conn_args.log2_ring_size = 4;
    memcpy((char *)rx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
           sizeof(rx_ctx->memif_conn_args.interface_name));
    rx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    rx_ctx->shm_buf_num = 1 << rx_ctx->memif_conn_args.log2_ring_size;
    rx_ctx->shm_bufs = (shm_buf_info_t *)calloc(rx_ctx->shm_buf_num, sizeof(shm_buf_info_t));
    if (!rx_ctx->shm_bufs) {
        ERROR("%s Failed to allocate shared memory buffers", __func__);
        return -ENOMEM;
    }

    INFO("create memif interface.");
    err = memif_create(&rx_ctx->memif_conn, &rx_ctx->memif_conn_args, rx_rdma_on_connect,
                       rx_rdma_on_disconnect, rx_on_receive, rx_ctx);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(err));
        free(rx_ctx->shm_bufs);
        return -1;
    }

    /* Start the MemIF event loop. */
    err = pthread_create(&rx_ctx->memif_event_thread, NULL, memif_event_loop,
                         rx_ctx->memif_conn_args.socket);
    if (err < 0) {
        printf("%s(%d), thread create fail\n", __func__, err);
        free(rx_ctx->shm_bufs);
        return -1;
    }

    return 0;
}

int tx_rdma_shm_init(tx_rdma_session_context_t *tx_ctx, memif_ops_t *memif_ops)
{
    memif_ops_t default_memif_ops = { 0 };
    struct stat st = { 0 };
    int err;

    if (tx_ctx == NULL) {
        printf("%s, fail to initialize share memory.\n", __func__);
        return -1;
    }

    bzero(tx_ctx->memif_socket_args.app_name, sizeof(tx_ctx->memif_socket_args.app_name));
    bzero(tx_ctx->memif_socket_args.path, sizeof(tx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strlcpy(default_memif_ops.app_name, "mcm_tx", sizeof(default_memif_ops.app_name));
        strlcpy(default_memif_ops.interface_name, "mcm_tx",
                sizeof(default_memif_ops.interface_name));
        strlcpy(default_memif_ops.socket_path, "/run/mcm/mcm_tx_memif.sock",
                sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strlcpy(tx_ctx->memif_socket_args.app_name, memif_ops->app_name,
            sizeof(tx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strlcpy(tx_ctx->memif_socket_args.path, memif_ops->socket_path,
            sizeof(tx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        if (stat("/run/mcm", &st) == -1) {
            err = mkdir("/run/mcm", 0666);
            if (err != 0) {
                ERROR("Create directory for MemIF socket.");
                return -1;
            }
        }
        unlink(tx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    err = memif_create_socket(&tx_ctx->memif_socket, &tx_ctx->memif_socket_args, NULL);
    if (err != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(err));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    tx_ctx->shm_ready = ATOMIC_VAR_INIT(false);
    tx_ctx->memif_conn_args.socket = tx_ctx->memif_socket;
    tx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    tx_ctx->memif_conn_args.buffer_size = (uint32_t)tx_ctx->transfer_size;
    tx_ctx->memif_conn_args.log2_ring_size = 4;
    snprintf((char *)tx_ctx->memif_conn_args.interface_name,
             sizeof(tx_ctx->memif_conn_args.interface_name), "%s", memif_ops->interface_name);
    tx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    INFO("Create memif interface.");
    err = memif_create(&tx_ctx->memif_conn, &tx_ctx->memif_conn_args, tx_rdma_on_connect,
                       tx_rdma_on_disconnect, tx_rdma_on_receive, tx_ctx);
    if (err != MEMIF_ERR_SUCCESS) {
        ERROR("memif_create: %s", memif_strerror(err));
        return -1;
    }

    /* Start the MemIF event loop. */
    err = pthread_create(&tx_ctx->memif_event_thread, NULL, memif_event_loop,
                         tx_ctx->memif_conn_args.socket);
    if (err < 0) {
        ERROR("%s(%d), thread create failed\n", __func__, err);
        return -1;
    }

    return 0;
}

static int rx_shm_deinit(rx_rdma_session_context_t *rx_ctx)
{
    int err;

    if (rx_ctx == NULL) {
        ERROR("%s, Illegal parameter.", __func__);
        return -1;
    }

    err = pthread_cancel(rx_ctx->memif_event_thread);
    if (!err)
        err = pthread_join(rx_ctx->memif_event_thread, NULL);
    if (err && err != ESRCH)
        ERROR("%s: Error joining thread: %s", __func__, strerror(err));

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

static int tx_shm_deinit(tx_rdma_session_context_t *tx_ctx)
{
    int err;

    if (tx_ctx == NULL) {
        ERROR("%s, Illegal parameter.", __func__);
        return -1;
    }

    err = pthread_cancel(tx_ctx->memif_event_thread);
    if (!err)
        err = pthread_join(tx_ctx->memif_event_thread, NULL);
    if (err && err != ESRCH)
        ERROR("%s: Error joining thread: %s", __func__, strerror(err));

    /* free-up resources */
    memif_delete(&tx_ctx->memif_conn);
    memif_delete_socket(&tx_ctx->memif_socket);

    /* unlink socket file */
    if (tx_ctx->memif_conn_args.is_master && tx_ctx->memif_socket_args.path[0] != '@') {
        unlink(tx_ctx->memif_socket_args.path);
    }

    return 0;
}

static void handle_sent_buffers(tx_rdma_session_context_t *s)
{
    shm_buf_info_t *buf_info;
    uint16_t bursted_buf_num;
    int err;

    err = ep_txcq_read(s->ep_ctx, 1);
    if (err) {
        if (err != -EAGAIN)
            INFO("%s ep_txcq_read: %s", __func__, strerror(err));
        return;
    }
    s->fb_send++;

    err = memif_refill_queue(s->memif_conn, 0, 1, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));
}

static void *tx_rdma_ep_thread(void *arg)
{
    tx_rdma_session_context_t *s_ctx = (tx_rdma_session_context_t *)arg;
    memif_region_details_t region;
    int err = 0;

    while (!atomic_load_explicit(&s_ctx->shm_ready, memory_order_acquire) && !s_ctx->stop)
        usleep(1000);

    INFO("%s(%d), TX RDMA thread started\n", __func__, s_ctx->idx);
    while (!s_ctx->stop) {
        if (!atomic_load_explicit(&s_ctx->shm_ready, memory_order_acquire))
            continue;
        handle_sent_buffers(s_ctx);
    }

    return NULL;
}

/* TX: Create RDMA session */
tx_rdma_session_context_t *rdma_tx_session_create(libfabric_ctx *dev_handle, rdma_s_ops_t *opts,
                                                  memif_ops_t *memif_ops)
{
    tx_rdma_session_context_t *tx_ctx = NULL;
    ep_cfg_t ep_cfg = { 0 };
    int err;

    if (!dev_handle || !opts || !memif_ops) {
        ERROR("%s, A input parameter is NULL", __func__);
        return NULL;
    }

    tx_ctx = calloc(1, sizeof(tx_rdma_session_context_t));
    if (tx_ctx == NULL) {
        ERROR("%s, TX session contex allocation failed\n", __func__);
        return NULL;
    }

    tx_ctx->transfer_size = opts->transfer_size;
    tx_ctx->rdma_ctx = dev_handle;
    tx_ctx->stop = false;

    ep_cfg.rdma_ctx = tx_ctx->rdma_ctx;
    ep_cfg.local_addr = opts->local_addr;
    ep_cfg.remote_addr = opts->remote_addr;
    ep_cfg.dir = opts->dir;

    err = ep_init(&tx_ctx->ep_ctx, &ep_cfg);
    if (err) {
        ERROR("%s, failed to initialize libfabric's end point.\n", __func__);
        goto exit_dealloc;
    }

    err = tx_rdma_shm_init(tx_ctx, memif_ops);
    if (err < 0) {
        ERROR("%s, failed to initialize share memory.\n", __func__);
        goto exit_deinit_ep;
    }

    err = pthread_create(&tx_ctx->ep_thread, NULL, tx_rdma_ep_thread, tx_ctx);
    if (err < 0) {
        ERROR("%s, Endpoint thread %d create failed: %s\n", __func__, tx_ctx->idx, strerror(err));
        goto exit_deinit_shm;
    }

    return tx_ctx;

exit_deinit_shm:
    tx_shm_deinit(tx_ctx);
exit_deinit_ep:
    ep_destroy(&tx_ctx->ep_ctx);
exit_dealloc:
    free(tx_ctx);
    return NULL;
}

static shm_buf_info_t *get_free_shm_buf(rx_rdma_session_context_t *s)
{
    uint32_t i;
    for (i = 0; i < s->shm_buf_num; i++) {
        if (!s->shm_bufs[i].used) {
            return &s->shm_bufs[i];
        }
    }
    return NULL;
}

static int pass_empty_buf_to_libfabric(rx_rdma_session_context_t *s)
{
    int err;
    shm_buf_info_t *buf_info = NULL;
    uint32_t buf_size = s->transfer_size;
    uint16_t rx_buf_num = 0;

    buf_info = get_free_shm_buf(s);
    if (!buf_info)
        return -ENOMEM;

    err = memif_buffer_alloc(s->memif_conn, 0, &buf_info->shm_buf, 1, &rx_buf_num, buf_size);
    if (err != MEMIF_ERR_SUCCESS)
        return -ENOMEM;

    buf_info->used = true;

    err = ep_recv_buf(s->ep_ctx, buf_info->shm_buf.data, buf_size, buf_info);
    if (err) {
        ERROR("%s ep_recv_buf failed with errno: %s", __func__, fi_strerror(err));
        return err;
    }
    return 0;
}

static void handle_received_buffers(rx_rdma_session_context_t *s)
{
    shm_buf_info_t *buf_info;
    int err;
    uint16_t bursted_buf_num;

    err = ep_rxcq_read(s->ep_ctx, (void **)&buf_info, 1);
    if (err) {
        if (err != -EAGAIN)
            INFO("%s ep_rxcq_read: %s", __func__, strerror(err));
        return;
    }
    s->fb_recv++;

    err = memif_tx_burst(s->memif_conn, 0, &buf_info->shm_buf, 1, &bursted_buf_num);
    if (err != MEMIF_ERR_SUCCESS && bursted_buf_num != 1) {
        INFO("%s memif_tx_burst: %s", __func__, memif_strerror(err));
        return;
    }
    buf_info->used = false;
}

static void *rx_rdma_ep_thread(void *arg)
{
    rx_rdma_session_context_t *s_ctx = (rx_rdma_session_context_t *)arg;
    memif_region_details_t region;
    int err = 0;

    while (!atomic_load_explicit(&s_ctx->shm_ready, memory_order_acquire) && !s_ctx->stop)
        usleep(1000);

    INFO("%s(%d), RX RDMA thread started\n", __func__, s_ctx->idx);
    while (!s_ctx->stop) {
        if (!atomic_load_explicit(&s_ctx->shm_ready, memory_order_acquire))
            continue;
        while (!pass_empty_buf_to_libfabric(s_ctx)) {}
        handle_received_buffers(s_ctx);
    }

    return NULL;
}

/* RX: Create RDMA session */
rx_rdma_session_context_t *rdma_rx_session_create(libfabric_ctx *dev_handle, rdma_s_ops_t *opts,
                                                  memif_ops_t *memif_ops)
{
    rx_rdma_session_context_t *rx_ctx = NULL;
    ep_cfg_t ep_cfg = { 0 };
    int err;

    if (!dev_handle || !opts || !memif_ops) {
        ERROR("%s, A input parameter is NULL", __func__);
        return NULL;
    }

    rx_ctx = calloc(1, sizeof(rx_rdma_session_context_t));
    if (!rx_ctx) {
        ERROR("%s, TX session contex allocation failed\n", __func__);
        return NULL;
    }

    rx_ctx->transfer_size = opts->transfer_size;
    rx_ctx->rdma_ctx = dev_handle;
    rx_ctx->stop = false;

    ep_cfg.rdma_ctx = rx_ctx->rdma_ctx;
    ep_cfg.local_addr = opts->local_addr;
    ep_cfg.remote_addr = opts->remote_addr;
    ep_cfg.dir = opts->dir;

    err = ep_init(&rx_ctx->ep_ctx, &ep_cfg);
    if (err) {
        ERROR("%s, failed to initialize libfabric's end point.\n", __func__);
        goto exit_dealloc;
    }

    err = rx_rdma_shm_init(rx_ctx, memif_ops);
    if (err < 0) {
        ERROR("%s, Failed to initialize share memory.\n", __func__);
        goto exit_deinit_ep;
    }

    err = pthread_create(&rx_ctx->ep_thread, NULL, rx_rdma_ep_thread, rx_ctx);
    if (err < 0) {
        ERROR("%s, Endpoint thread %d create failed: %s\n", __func__, rx_ctx->idx, strerror(err));
        goto exit_deinit_shm;
    }

    return rx_ctx;

exit_deinit_shm:
    rx_shm_deinit(rx_ctx);
exit_deinit_ep:
    ep_destroy(&rx_ctx->ep_ctx);
exit_dealloc:
    free(rx_ctx);
    return NULL;
}

void rdma_rx_session_stop(rx_rdma_session_context_t *rx_ctx)
{
    int err;

    if (rx_ctx == NULL) {
        ERROR("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx->stop = true;

    err = pthread_join(rx_ctx->ep_thread, NULL);
    if (err && err != ESRCH) {
        ERROR("%s: Error joining thread: %s\n", __func__, strerror(err));
    }
}

void rdma_rx_session_destroy(rx_rdma_session_context_t **p_rx_ctx)
{
    rx_rdma_session_context_t *rx_ctx = NULL;
    int err;

    if (p_rx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    rx_ctx = *p_rx_ctx;
    err = ep_destroy(&rx_ctx->ep_ctx);
    if (err < 0) {
        printf("%s, ep free failed\n", __func__);
        return;
    }

    rx_shm_deinit(rx_ctx);

    /* Free shared memory resource. */
    free(rx_ctx);
}

void rdma_tx_session_stop(tx_rdma_session_context_t *tx_ctx)
{
    int err;

    if (tx_ctx == NULL) {
        ERROR("%s: invalid parameter", __func__);
        return;
    }

    tx_ctx->stop = true;

    err = pthread_join(tx_ctx->ep_thread, NULL);
    if (err && err != ESRCH) {
        ERROR("%s: Error joining thread: %s\n", __func__, strerror(err));
    }
}

void rdma_tx_session_destroy(tx_rdma_session_context_t **p_tx_ctx)
{
    tx_rdma_session_context_t *tx_ctx = NULL;
    int err;

    if (p_tx_ctx == NULL) {
        printf("%s: invalid parameter\n", __func__);
        return;
    }

    tx_ctx = *p_tx_ctx;
    err = ep_destroy(&tx_ctx->ep_ctx);
    if (err < 0) {
        printf("%s, ep free failed\n", __func__);
        return;
    }

    tx_shm_deinit(tx_ctx);

    /* Free shared memory resource. */
    free(tx_ctx);
}
