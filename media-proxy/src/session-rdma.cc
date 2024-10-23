#include <pthread.h>
#include <sys/stat.h>
#include <bsd/string.h>

#include "libfabric_dev.h"
#include "session-rdma.h"

void tx_rdma_session::handle_sent_buffers()
{
    int err = ep_txcq_read(ep_ctx, 1);
    if (err) {
        if (err != -EAGAIN)
            INFO("%s ep_txcq_read: %s", __func__, strerror(err));
        return;
    }
    fb_send++;

    err = memif_refill_queue(memif_conn, 0, 1, 0);
    if (err != MEMIF_ERR_SUCCESS)
        INFO("memif_refill_queue: %s", memif_strerror(err));
}

void tx_rdma_session::frame_thread()
{
    while (!atomic_load_explicit(&shm_ready, memory_order_acquire) && !stop)
        usleep(1000);

    INFO("%s, TX RDMA thread started\n", __func__);
    while (!stop) {
        if (!atomic_load_explicit(&shm_ready, memory_order_acquire))
            continue;
        handle_sent_buffers();
    }
}

/* TX: Create RDMA session */
tx_rdma_session::tx_rdma_session(libfabric_ctx *dev_handle, rdma_s_ops_t *opts,
                                 memif_ops_t *memif_ops)
    : rdma_ctx(0), ep_ctx(0), stop(false), frame_thread_handle(0), fb_send(0)
{
    if (!dev_handle || !opts || !memif_ops) {
        cleanup();
        throw invalid_argument("Input parameters cannot be null");
    }

    rdma_ctx = dev_handle;
    transfer_size = opts->transfer_size;

    ep_cfg_t ep_cfg = {
        .rdma_ctx = rdma_ctx,
        .remote_addr = opts->remote_addr,
        .local_addr = opts->local_addr,
        .dir = opts->dir,
    };

    int err = ep_init(&ep_ctx, &ep_cfg);
    if (err) {
        cleanup();
        throw runtime_error("Failed to initialize libfabric's end point");
    }

    err = shm_init(memif_ops, transfer_size, 4);
    if (err < 0) {
        cleanup();
        throw runtime_error("Failed to to initialize shared memory");
    }

    frame_thread_handle = new std::thread(&tx_rdma_session::frame_thread, this);
    if (!frame_thread_handle) {
        cleanup();
        throw runtime_error("Failed to create thread");
    }
}

void tx_rdma_session::cleanup()
{
    if (ep_ctx) {
        ep_destroy(&ep_ctx);
        ep_ctx = 0;
    }

    if (frame_thread_handle) {
        frame_thread_handle->join();
        delete frame_thread_handle;
        frame_thread_handle = 0;
    }
}

tx_rdma_session::~tx_rdma_session()
{
    INFO("%s, fb_send %d\n", __func__, fb_send);
    cleanup();
}

shm_buf_info_t *rx_rdma_session::get_free_shm_buf()
{
    for (uint32_t i = 0; i < shm_buf_num; i++) {
        if (!shm_bufs[i].used) {
            return &shm_bufs[i];
        }
    }
    return NULL;
}

int rx_rdma_session::pass_empty_buf_to_libfabric()
{
    int err;
    shm_buf_info_t *buf_info = NULL;
    uint16_t rx_buf_num = 0;

    buf_info = get_free_shm_buf();
    if (!buf_info)
        return -ENOMEM;

    err = memif_buffer_alloc(memif_conn, 0, &buf_info->shm_buf, 1, &rx_buf_num, transfer_size);
    if (err != MEMIF_ERR_SUCCESS)
        return -ENOMEM;

    buf_info->used = true;

    err = ep_recv_buf(ep_ctx, buf_info->shm_buf.data, transfer_size, buf_info);
    if (err) {
        ERROR("%s ep_recv_buf failed with errno: %s", __func__, fi_strerror(err));
        return err;
    }
    return 0;
}

void rx_rdma_session::handle_received_buffers()
{
    shm_buf_info_t *buf_info;
    int err;
    uint16_t bursted_buf_num;

    err = ep_rxcq_read(ep_ctx, (void **)&buf_info, 1);
    if (err) {
        if (err != -EAGAIN)
            INFO("%s ep_rxcq_read: %s", __func__, strerror(err));
        return;
    }
    fb_recv++;

    err = memif_tx_burst(memif_conn, 0, &buf_info->shm_buf, 1, &bursted_buf_num);
    if (err != MEMIF_ERR_SUCCESS && bursted_buf_num != 1) {
        INFO("%s memif_tx_burst: %s", __func__, memif_strerror(err));
        return;
    }
    buf_info->used = false;
}

void rx_rdma_session::frame_thread()
{
    while (!atomic_load_explicit(&shm_ready, memory_order_acquire) && !stop)
        usleep(1000);

    INFO("%s, RX RDMA thread started\n", __func__);
    while (!stop) {
        if (!atomic_load_explicit(&shm_ready, memory_order_acquire))
            continue;
        while (!pass_empty_buf_to_libfabric()) {
        }
        handle_received_buffers();
    }
}

/* RX: Create RDMA session */
rx_rdma_session::rx_rdma_session(libfabric_ctx *dev_handle, rdma_s_ops_t *opts,
                                 memif_ops_t *memif_ops)
    : rdma_ctx(0), ep_ctx(0), stop(false), frame_thread_handle(0), fb_recv(0), shm_bufs(0)
{
    if (!dev_handle || !opts || !memif_ops) {
        cleanup();
        throw invalid_argument("Input parameters cannot be null");
    }

    transfer_size = opts->transfer_size;
    rdma_ctx = dev_handle;

    ep_cfg_t ep_cfg = {
        .rdma_ctx = rdma_ctx,
        .remote_addr = opts->remote_addr,
        .local_addr = opts->local_addr,
        .dir = opts->dir,
    };

    int err = ep_init(&ep_ctx, &ep_cfg);
    if (err) {
        cleanup();
        throw runtime_error("Failed to initialize libfabric's end point");
    }

    shm_buf_num = 1 << 4;
    shm_bufs = (shm_buf_info_t *)calloc(shm_buf_num, sizeof(shm_buf_info_t));
    if (!shm_bufs) {
        cleanup();
        throw runtime_error("Failed to allocate memory");
    }

    err = shm_init(memif_ops, transfer_size, 4);
    if (err < 0) {
        cleanup();
        throw runtime_error("Failed to initialize shared memory");
    }

    frame_thread_handle = new std::thread(&rx_rdma_session::frame_thread, this);
    if (!frame_thread_handle) {
        cleanup();
        throw runtime_error("Failed to create thread");
    }
}

void rx_rdma_session::cleanup()
{
    if (ep_ctx) {
        ep_destroy(&ep_ctx);
        ep_ctx = 0;
    }

    if (shm_bufs) {
        free(shm_bufs);
        shm_bufs = 0;
    }

    if (frame_thread_handle) {
        frame_thread_handle->join();
        delete frame_thread_handle;
        frame_thread_handle = 0;
    }
}

rx_rdma_session::~rx_rdma_session()
{
    INFO("%s, fb_recv %d\n", __func__, fb_recv);
    cleanup();
}

void rx_rdma_session::session_stop() { stop = true; }

void tx_rdma_session::session_stop() { stop = true; }

int tx_rdma_session::on_receive_cb(memif_conn_handle_t conn, uint16_t qid)
{
    memif_buffer_t shm_bufs = {0};
    uint16_t buf_num = 0;
    int err = 0;

    if (stop) {
        INFO("TX session already stopped.");
        return -EINVAL;
    }

    /* receive packets from the shared memory */
    err = memif_rx_burst(conn, qid, &shm_bufs, 1, &buf_num);
    if (err != MEMIF_ERR_SUCCESS && err != MEMIF_ERR_NOBUF) {
        INFO("memif_rx_burst: %s", memif_strerror(err));
        return err;
    }

    err = ep_send_buf(ep_ctx, shm_bufs.data, shm_bufs.len);
    if (err) {
        ERROR("ep_send_buf failed with: %s", fi_strerror(err));
        return err;
    }

    return 0;
}

int tx_rdma_session::on_connect_cb(memif_conn_handle_t conn)
{
    memif_region_details_t region;

    int err = memif_get_buffs_region(conn, &region);
    if (err) {
        ERROR("%s, Getting memory buffers from memif failed. \n", __func__);
        return err;
    }

    err = ep_reg_mr(ep_ctx, region.addr, region.size);
    if (errno) {
        ERROR("%s, ep_reg_mr failed: %s\n", __func__, fi_strerror(err));
        return err;
    }

    return session::on_connect_cb(conn);
}

int rx_rdma_session::on_connect_cb(memif_conn_handle_t conn)
{
    memif_region_details_t region;

    int err = memif_get_buffs_region(conn, &region);
    if (err) {
        ERROR("%s, Getting memory buffers from memif failed. \n", __func__);
        return err;
    }

    err = ep_reg_mr(ep_ctx, region.addr, region.size);
    if (errno) {
        ERROR("%s, ep_reg_mr failed: %s\n", __func__, fi_strerror(err));
        return err;
    }

    return session::on_connect_cb(conn);
}
