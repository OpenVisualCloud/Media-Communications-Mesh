#include <queue>
#include "conn_rdma.h"
#include <netinet/in.h>   // for sockaddr_in
#include <arpa/inet.h>    // for ntohs/htons

namespace mesh::connection {

std::atomic<int> Rdma::active_connections(0);

Rdma::Rdma()
  : ep_ctxs{}    
  , init(false)
  , trx_sz(0)
  , m_dev_handle(nullptr)
  , buffer_block(nullptr)
  , queue_size(0)
{
    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
    ++active_connections;
}

Rdma::~Rdma() {
    on_shutdown(context::Background());
    --active_connections;
    deinit_rdma_if_needed(m_dev_handle);
}

void Rdma::deinit_rdma_if_needed(libfabric_ctx *m_dev_handle)
{
    static std::mutex deinit_mutex;
    std::lock_guard<std::mutex> lock(deinit_mutex);

    if (active_connections == 0 && m_dev_handle) {
        int ret = libfabric_dev_ops.rdma_deinit(&m_dev_handle);
        if (ret) {
            log::error("Failed to deinitialize RDMA device")("error", fi_strerror(-ret));
        } else {
            log::info("RDMA device successfully deinitialized");
        }
        m_dev_handle = nullptr;
    }
}

void Rdma::notify_cq_event() {
    std::lock_guard<std::mutex> lock(cq_mutex);
    event_ready = true;
    cq_cv.notify_one();
}

void Rdma::init_buf_available() { buf_available = false; }

void Rdma::notify_buf_available()
{
    buf_available.store(true, std::memory_order_release);
    buf_available.notify_one();
}

void Rdma::wait_buf_available()
{
    buf_available.wait(false, std::memory_order_acquire);
    buf_available = false;
}

// Add to queue
Result Rdma::add_to_queue(void *element)
{
    if (!element) {
        return set_result(Result::error_bad_argument);
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        buffer_queue.push(element);
    }
    notify_buf_available(); // Notify availability

    return Result::success;
}

// Consume from queue
Result Rdma::consume_from_queue(context::Context& ctx, void **element)
{
    std::unique_lock<std::mutex> lock(queue_mutex);

    if (ctx.cancelled()) {
        *element = nullptr; // Ensure element remains nullptr
        return Result::error_context_cancelled;
    }

    if (buffer_queue.empty()) {
        return Result::error_no_buffer;
    }

    *element = buffer_queue.front();
    buffer_queue.pop();

    return Result::success;
}

/**
 * @brief Initializes the RDMA buffer queue with a specified number of pre-allocated elements.
 * 
 * Allocates a single contiguous memory block, aligns buffer sizes to the page size, and divides 
 * the block into individual buffers. Pushes these buffers into the queue for future use.
 * Ensures proper cleanup on failure or reinitialization attempts.
 * 
 * @param capacity The number of buffers to allocate within continuous virtual memory.
 * @param trx_sz The size of each buffer (aligned to the page size).
 * @return Result::success on successful initialization, or an error result if arguments are invalid 
 *         or memory allocation fails.
 */
Result Rdma::init_queue_with_elements(size_t capacity, size_t trx_sz)
{
    if (trx_sz == 0 || trx_sz > MAX_BUFFER_SIZE) {
        log::error("Invalid transfer size provided for RDMA buffer allocation")("trx_sz", trx_sz);
        return Result::error_bad_argument;
    }

    std::lock_guard<std::mutex> lock(queue_mutex);

    // Check if already initialized
    if (!buffer_queue.empty()) {
        log::error("RDMA buffer queue already initialized");
        return Result::error_already_initialized;
    }

    // Calculate total memory size needed and align to page size
    size_t aligned_trx_sz = ((trx_sz + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    size_t total_size = capacity * aligned_trx_sz;

    // Allocate a single contiguous memory block
    void *memory_block = std::aligned_alloc(PAGE_SIZE, total_size);
    if (!memory_block) {
        log::error("RDMA failed to allocate a single memory block")("total_size", total_size);
        return Result::error_out_of_memory;
    }

    // Zero-initialize the entire memory block
    std::memset(memory_block, 0, total_size);

    // Divide the memory block into individual buffers
    char *base_ptr = static_cast<char *>(memory_block);
    for (size_t i = 0; i < capacity; ++i) {
        void *buf = base_ptr + i * aligned_trx_sz;
        buffer_queue.push(buf);
    }

    // Store the base memory block for cleanup
    buffer_block = memory_block;

    return Result::success;
}

// Cleanup the buffer queue
void Rdma::cleanup_queue()
{
    if (buffer_block) {
        // Free the entire memory block
        std::free(buffer_block);
        buffer_block = nullptr;
    }
    while (!buffer_queue.empty()) {
        buffer_queue.pop();
    }
}

/**
 * @brief Configures the RDMA connection with the provided parameters.
 * 
 * Sets up the RDMA transfer size, connection kind, endpoint configuration,
 * and queue size based on the input request. Ensures that the kind-direction
 * pairing is valid (e.g., receiver with RX direction). Updates the connection
 * state to "configured" upon success.
 * 
 * @param ctx The operation context.
 * @param request The connection parameters including addresses and RDMA arguments.
 * @param dev_handle Reference to the RDMA device handle.
 * @param kind The type of connection (receiver or transmitter).
 * @param dir The data transfer direction (RX or TX).
 * @return Result::success on successful configuration, or an error result if arguments are invalid.
 */
Result Rdma::configure(context::Context& ctx, const mcm_conn_param& request,
                       libfabric_ctx *& dev_handle)
{

    trx_sz = request.payload_args.rdma_args.transfer_size;
    // --- validate & assign RDMA provider ---
    if (request.payload_args.rdma_args.provider
        && *request.payload_args.rdma_args.provider) {
        rdma_provider = request.payload_args.rdma_args.provider;
    } else {
        log::warn("RDMA provider not specified, defaulting to 'verbs'");
        rdma_provider = "verbs";
    }

    // --- validate & assign number of endpoints ---
    uint32_t neps = request.payload_args.rdma_args.num_endpoints;
    if (neps >= 1 && neps <= 8) {
        rdma_num_eps = neps;
    } else {
        log::warn(
            "RDMA num_endpoints {} out of valid range [1..8], defaulting to 1",
            neps
        );
        rdma_num_eps = 1;
    }

    _kind = kind();

    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
    std::memcpy(&ep_cfg.remote_addr, &request.remote_addr, sizeof(request.remote_addr));
    std::memcpy(&ep_cfg.local_addr, &request.local_addr, sizeof(request.local_addr));
    
    ep_cfg.dir = _kind == Kind::receiver ? direction::RX : direction::TX;

    m_dev_handle = dev_handle;

    queue_size = request.payload_args.rdma_args.queue_size;

    set_state(ctx, State::configured);
    return Result::success;
}

/**
 * @brief Establishes an RDMA connection by initializing the device, endpoint, buffer queue,
 * and starting necessary threads for processing RDMA operations.
 *
 * Ensures proper cleanup and state updates on failure. Returns success if all steps
 * (device initialization, endpoint setup, buffer queue creation, and endpoint configuration)
 * complete successfully.
 *
 * @param ctx The operation context.
 * @return Result::success on success, or an appropriate error result on failure.
 */
Result Rdma::on_establish(context::Context& ctx)
{
    // 1) Already initialized?
    if (init) {
        log::error("RDMA device is already initialized")("state", "initialized");
        set_state(ctx, State::active);
        return Result::error_already_initialized;
    }

    // 2) Unblock buffer threads
    init_buf_available();

    int    ret;
    Result res;

    // 3) Initialize the RDMA device if needed
    if (!m_dev_handle) {
        m_dev_handle = (libfabric_ctx*)calloc(1, sizeof(libfabric_ctx));
        if (!m_dev_handle) {
            log::error("Failed to allocate RDMA context")("error", strerror(errno));
            return Result::error_out_of_memory;
        }
        _kind = kind();
        m_dev_handle->kind        = (_kind == Kind::receiver
                                     ? FI_KIND_RECEIVER
                                     : FI_KIND_TRANSMITTER);        
        m_dev_handle->local_ip    = ep_cfg.local_addr.ip;
        m_dev_handle->local_port  = ep_cfg.local_addr.port;
        m_dev_handle->remote_ip   = ep_cfg.remote_addr.ip;
        m_dev_handle->remote_port = ep_cfg.remote_addr.port;
        m_dev_handle->provider_name = strdup(rdma_provider.c_str());
        ret = libfabric_dev_ops.rdma_init(&m_dev_handle);

        if (ret) {
            log::error("Failed to initialize RDMA device")("ret", ret)
                     ("error", fi_strerror(-ret));
            free(m_dev_handle);
            m_dev_handle = nullptr;
            set_state(ctx, State::closed);
            return Result::error_initialization_failed;
        }
        log::info("RDMA device successfully initialized");
    }

    auto bump_sock = [](sockaddr_in* sa, uint32_t delta)
    {
        unsigned short p = ntohs(sa->sin_port);
        sa->sin_port    = htons(static_cast<unsigned short>(p + delta));
    };

    auto bump_port_str = [](char dst[6], const char src[6], uint32_t delta)
    {
        unsigned short p = static_cast<unsigned short>(std::atoi(src));
        std::snprintf(dst, 6, "%u", p + delta);
    };

    /* --------------------------------------------------------------------
    * 4) Allocate per‐EP arrays by rdma_num_eps
    * ------------------------------------------------------------------*/
    std::vector<libfabric_ctx*>  devs(rdma_num_eps, nullptr);
    std::vector<fi_info*>        info_dups(rdma_num_eps, nullptr);

    // cleanup helper: free all clones [1..n)
    auto cleanup_clones = [&](std::size_t upto) {
        for (std::size_t j = 1; j < upto; ++j) {
            if (info_dups[j]) {
                fi_freeinfo(info_dups[j]);
                info_dups[j] = nullptr;
            }
            if (devs[j]) {
                free(devs[j]);
                devs[j] = nullptr;
            }
        }
    };
    
    // resize our member‐vector of EP pointers
    ep_ctxs.resize(rdma_num_eps);

    /* EP-0 re-uses the context already created in rdma_init() */
    devs[0] = m_dev_handle;            // never null
    info_dups[0] = m_dev_handle->info; // keep for uniform cleanup

    /* create clones for EP-1 … EP-N */
    for (uint32_t i = 1; i < rdma_num_eps; ++i) {
        fi_info* dup = fi_dupinfo(m_dev_handle->info);
        if (!dup) {
            log::error("fi_dupinfo failed for EP %u", i)("kind", kind2str(_kind));
            // free any earlier clones
            cleanup_clones(i);
            set_state(ctx, State::closed);
            return Result::error_initialization_failed;
        }
        info_dups[i] = dup;

        if (dup->src_addr && dup->src_addrlen == sizeof(sockaddr_in))
            bump_sock(reinterpret_cast<sockaddr_in*>(dup->src_addr), i);
        if (dup->dest_addr && dup->dest_addrlen == sizeof(sockaddr_in))
            bump_sock(reinterpret_cast<sockaddr_in*>(dup->dest_addr), i);

        devs[i] = static_cast<libfabric_ctx*>(calloc(1, sizeof(libfabric_ctx)));
        if (!devs[i]) {
            log::error("Failed to allocate RDMA context clone for EP %u", i)("error",
                                                                             strerror(errno));
            fi_freeinfo(dup);
            // free any earlier clones
            cleanup_clones(i);
            set_state(ctx, State::closed);
            return Result::error_out_of_memory;
        }
        *devs[i] = *m_dev_handle; // shallow copy
        devs[i]->info = dup;
        devs[i]->is_initialized = true;
    }

    /* ---------- cfgs ---------------------------------------------------- */
    std::vector<ep_cfg_t> cfgs(rdma_num_eps, ep_cfg);
    for (uint32_t i = 1; i < rdma_num_eps; ++i) {
        bump_port_str(cfgs[i].local_addr.port,  cfgs[0].local_addr.port,  i);
        bump_port_str(cfgs[i].remote_addr.port, cfgs[0].remote_addr.port, i);
    }

    /* ---------- bring up endpoints ------------------------------------- */
    for (uint32_t i = 0; i < rdma_num_eps; ++i) {
        cfgs[i].rdma_ctx = devs[i];
        if (i > 0) {
            cfgs[i].shared_rx_cq = ep_ctxs[0]->cq_ctx.cq; // use EP-0 CQ for RX
        } else {
            cfgs[i].shared_rx_cq = nullptr; // no shared CQ for EP-0
        }
        int ret = libfabric_ep_ops.ep_init(&ep_ctxs[i], &cfgs[i]);
        if (ret) {
            log::error("Failed to initialize RDMA endpoint #%u", i)("ret", ret)("error",
                       fi_strerror(-ret));
            // destroy all created endpoints
            for (auto& e : ep_ctxs) {
                if (e) {
                    libfabric_ep_ops.ep_destroy(&e);
                }
            }
            cleanup_clones(i + 1);
            set_state(ctx, State::closed);
            return Result::error_initialization_failed;
        }
    }

    /* ---------- queue & MR section (no duplicate ‘res’) ----------------- */
    res = init_queue_with_elements(queue_size, trx_sz + TRAILER);
    if (res != Result::success) {
        log::error("Failed to initialise RDMA buffer queue")("trx_sz", trx_sz);
        for (auto &e : ep_ctxs) {
            if (e) {
                libfabric_ep_ops.ep_destroy(&e);
            }
        }
        cleanup_clones(rdma_num_eps);        
        set_state(ctx, State::closed);
        return res;
    }

    /* ------------------------------------------------------------------
    * 8) Register the **same** memory block on every endpoint
    * -----------------------------------------------------------------*/
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        std::size_t aligned_sz =                               /* one slot */
            (((trx_sz + TRAILER) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
        std::size_t total_size = queue_size * aligned_sz;

        for (auto* ep : ep_ctxs) {
            int rc = libfabric_ep_ops.ep_reg_mr(ep, buffer_block, total_size);
            if (rc) {
                log::error("Memory registration failed")("error", fi_strerror(-rc));
            for (auto &ee : ep_ctxs) {
                if (ee) {
                    libfabric_ep_ops.ep_destroy(&ee);
                }
            }
            cleanup_clones(rdma_num_eps);
                set_state(ctx, State::closed);
                return Result::error_memory_registration_failed;
            }
        }
    }

    /* ------------------------------------------------------------------
    * 9) Start TX / RX / CQ threads
    * -----------------------------------------------------------------*/
    init = true;
    res  = start_threads(ctx);
    if (res != Result::success) {
        log::error("Failed to start RDMA threads")("state", "closed");
        for (auto &e : ep_ctxs) if (e) libfabric_ep_ops.ep_destroy(&e);
        for (std::size_t i = 1; i < rdma_num_eps; ++i) {
            if (info_dups[i]) fi_freeinfo(info_dups[i]);
            if (devs[i])      free(devs[i]);
        }
        set_state(ctx, State::closed);
        return res;
    }

    /* ------------------------------------------------------------------
    * 10) Ready for traffic
    * -----------------------------------------------------------------*/
    set_state(ctx, State::active);
    return Result::success;

}



/**
 * @brief Cleans up RDMA resources and resets the state.
 *
 * This function destroys the RDMA endpoint context if it exists, cleans up the buffer queue,
 * and marks the RDMA as uninitialized. It ensures that all allocated resources are properly
 * released and the state is reset to allow for reinitialization if needed.
 *
 * @param ctx The context used for the cleanup operation.
 * @return Result indicating the success or failure of the cleanup operation.
 */
Result Rdma::on_shutdown(context::Context& ctx)
{
    // Note: this is a blocking call
    
    // Cancel `rdma_cq_thread` context
    rdma_cq_thread_ctx.cancel();
    // Cancel `process_buffers_thread` context
    process_buffers_thread_ctx.cancel();
    // Ensure buffer-related threads are unblocked
    notify_buf_available();

    try {
        // Check if `rdma_cq_thread` has stopped
        if (handle_rdma_cq_thread.joinable()) {
            handle_rdma_cq_thread.join();
        }
    } catch (const std::exception& e) {
        log::error("Exception caught while joining RDMA rdma_cq_thread")("error", e.what());
        return Result::error_general_failure;
    }

    try {
        // Check if `process_buffers_thread` has stopped
        if (handle_process_buffers_thread.joinable()) {
            handle_process_buffers_thread.join();
        }
    } catch (const std::exception& e) {
        log::error("Exception caught while joining RDMA process_buffers_thread")
                  ("error", e.what());
        return Result::error_general_failure;
    }

    // Clean up RDMA resources if initialized
    if (init) {
        cleanup_resources(ctx);
    }

    set_state(ctx, State::closed);

    return Result::success;
}

/**
 * @brief Handles the deletion of the RDMA connection.
 *
 * This function ensures that the RDMA connection is properly shut down
 * and resources are cleaned up before the object is deleted.
 *
 * @param ctx The context used for the delete operation.
 */
void Rdma::on_delete(context::Context& ctx)
{
    // Note: this is a blocking call
    on_shutdown(ctx);
}

/**
 * @brief Configures the RDMA endpoint by registering the memory block with the RDMA endpoint.
 *
 * This function ensures that the endpoint context is initialized and the memory block for the buffer queue
 * is allocated. It then registers it with the RDMA endpoint.
 *
 * @param ctx The context for the operation.
 * @return Result::success if the endpoint is successfully configured, otherwise an appropriate error code.
 */
Result Rdma::configure_endpoint(context::Context& ctx)
{
    // Ensure we have initialized endpoints
    for (size_t i = 0; i < ep_ctxs.size(); ++i) {
        if (!ep_ctxs[i]) {
            log::error("RDMA endpoint context #%zu is not initialized", i)
                ("kind", kind2str(_kind));
            return Result::error_wrong_state;
        }
    }

    std::lock_guard<std::mutex> lock(queue_mutex);

    if (!buffer_block) {
        log::error("Memory block for RDMA buffer queue is not allocated")
            ("kind", kind2str(_kind));
        return Result::error_out_of_memory;
    }

    // Total size in bytes of the contiguous buffer pool
    size_t buf_count    = buffer_queue.size();
    size_t aligned_sz = (((trx_sz + TRAILER) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    size_t total_size   = buf_count * aligned_sz;

    // Register the entire buffer block with each endpoint (QP)
    for (size_t i = 0; i < ep_ctxs.size(); ++i) {
        ep_ctx_t* e = ep_ctxs[i];
        int ret = libfabric_ep_ops.ep_reg_mr(e, buffer_block, total_size);
        if (ret) {
            log::error("Memory registration failed on endpoint #%zu", i)
                ("error", fi_strerror(-ret))
                ("kind", kind2str(_kind));
            return Result::error_memory_registration_failed;
        }
    }

    return Result::success;
}


Result Rdma::cleanup_resources(context::Context& ctx)
{
    // Destroy each endpoint (QP)
    for (size_t i = ep_ctxs.size(); i >= 1; --i) {
        ep_ctx_t*& e = ep_ctxs[i-1];
        if (e) {
            int err = libfabric_ep_ops.ep_destroy(&e);
            if (err) {
                log::error("Failed to destroy RDMA endpoint #%zu", i)
                    ("error", fi_strerror(-err))
                    ("kind", kind2str(_kind));
                return Result::error_general_failure;
            }
            e = nullptr;
        }
    }

    // Free and clear our buffer queue
    cleanup_queue();

    // Mark as uninitialized so we can re-establish if needed
    init = false;
    return Result::success;
}


} // namespace mesh::connection
