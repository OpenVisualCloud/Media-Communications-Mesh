#include "conn_rdma.h"
#include <queue>

namespace mesh {

namespace connection {

Rdma::Rdma() : ep_ctx(nullptr), init(false), trx_sz(0), mDevHandle(nullptr)
{
    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
}

Rdma::~Rdma() { shutdown_rdma(context::Background()); }

// Add to queue
Result Rdma::add_to_queue(void* element) {
    if (!element) {
        log::error("Cannot add a nullptr to the queue");
        return Result::error_bad_argument;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        buffer_queue.push(element);
    }

    log::debug("Element added to queue")("queue_size", buffer_queue.size());
    
    queue_cv.notify_one();
    return Result::success;
}

// Consume from queue
Result Rdma::consume_from_queue(context::Context& ctx, void** element) {
    std::unique_lock<std::mutex> lock(queue_mutex);

    if (ctx.cancelled()) {
        log::info("consume_from_queue: Context canceled, exiting wait");
        return Result::error_operation_cancelled;
    }

    if (buffer_queue.empty()) {
        log::error("Queue is empty");
        return Result::error_general_failure;
    }

    *element = buffer_queue.front();
    buffer_queue.pop();

    log::debug("Buffer consumed from queue")("queue_size", buffer_queue.size());
    return Result::success;
}

// Initialize the buffer queue with pre-allocated buffers
Result Rdma::init_queue_with_elements(size_t capacity, size_t trx_sz) {
    
    log::info("Buffer queue initialization before mutex");

    std::lock_guard<std::mutex> lock(queue_mutex);

    log::info("Buffer queue initialization after mutex");

    // Check if already initialized
    if (!buffer_queue.empty()) {
        log::error("Buffer queue already initialized");
        return Result::error_already_initialized;
    }

    // Pre-fill the queue with pre-allocated buffers
    for (size_t i = 0; i < capacity; ++i) {
        void* buf = std::calloc(1, trx_sz);
        if (!buf) {
            log::error("Failed to allocate memory for buffer")("index", i)("trx_sz", trx_sz);
            cleanup_queue(); // Free already allocated memory
            return Result::error_out_of_memory;
        }
        buffer_queue.push(buf);
    }

    log::info("Buffer queue initialized with elements")("capacity", capacity)("element_size", trx_sz);
    return Result::success;
}

// Cleanup the buffer queue
void Rdma::cleanup_queue() {
    std::lock_guard<std::mutex> lock(queue_mutex);

    while (!buffer_queue.empty()) {
        void* buf = buffer_queue.front();
        buffer_queue.pop();
        if (buf) {
            std::free(buf);
        }
    }

    log::info("Buffer queue cleaned up successfully");
}

Result Rdma::configure(context::Context& ctx, const mcm_conn_param& request,
                       const std::string& dev_port, libfabric_ctx*& dev_handle, Kind kind,
                       direction dir) {
    trx_sz = request.payload_args.rdma_args.transfer_size;
    _kind = kind;

    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
    std::memcpy(&ep_cfg.remote_addr, &request.remote_addr, sizeof(request.remote_addr));
    std::memcpy(&ep_cfg.local_addr, &request.local_addr, sizeof(request.local_addr));
    ep_cfg.dir = dir;

    mDevHandle = dev_handle;

    set_state(ctx, State::configured);
    return Result::success;
}

Result Rdma::on_establish(context::Context& ctx) {
    if (init) {
        log::error("RDMA device is already initialized")("state", "initialized");
        set_state(ctx, State::active);
        return Result::error_already_initialized;
    }

    int ret;
    Result res;

    if (!mDevHandle) {
        ret = libfabric_dev_ops.rdma_init(&mDevHandle);
        if (ret) {
            log::error("Failed to initialize RDMA device")("ret", ret);
            set_state(ctx, State::closed);
            return Result::error_initialization_failed;
        }
    }

    ep_cfg.rdma_ctx = mDevHandle;

    ret = libfabric_ep_ops.ep_init(&ep_ctx, &ep_cfg);
    if (ret) {
        log::error("Failed to initialize RDMA endpoint context")("error", fi_strerror(-ret));
        set_state(ctx, State::closed);
        return Result::error_initialization_failed;
    }

    res = init_queue_with_elements(16, trx_sz); // Assuming 16 slots in the buffer queue
    if (res != Result::success) {
        log::error("Failed to initialize buffer queue")("trx_sz", trx_sz);
        if (ep_ctx) {
            log::debug("Destroying endpoint due to queue initialization failure");
            libfabric_ep_ops.ep_destroy(&ep_ctx);
        }
        set_state(ctx, State::closed);
        return res;
    }

    log::info("Buffer queue initialized");
    // Configure RDMA endpoint
    res = configure_endpoint(ctx);
    if (res != Result::success) {
        log::error("RDMA configuring failed")("state", "closed");
        if (ep_ctx) {
            log::debug("Destroying endpoint due to configuration failure");
            libfabric_ep_ops.ep_destroy(&ep_ctx);
        }
        set_state(ctx, State::closed);
        return res;
    }

    init = true;
    res = start_threads(ctx);

    set_state(ctx, State::active);
    return Result::success;
}

Result Rdma::on_shutdown(context::Context& ctx)
{
    // Note: this is a blocking call
    shutdown_rdma(ctx);

    return Result::success;
}

void Rdma::on_delete(context::Context& ctx)
{
    // Note: this is a blocking call
    on_shutdown(ctx);
}

Result Rdma::configure_endpoint(context::Context& ctx)
{
    if (!ep_ctx) {
        log::error("Endpoint context is not initialized");
        return Result::error_wrong_state;
    }

    std::lock_guard<std::mutex> lock(queue_mutex);

    // Register all buffers in the queue with the RDMA endpoint
    std::queue<void*> temp_queue = buffer_queue; // Create a copy to iterate
    size_t index = 0;
    while (!temp_queue.empty()) {
        void* buffer = temp_queue.front();
        temp_queue.pop();

        if (!buffer) {
            log::error("Buffer queue element is not allocated")("index", index);
            return Result::error_out_of_memory;
        }

        int ret = libfabric_ep_ops.ep_reg_mr(ep_ctx, buffer, trx_sz);
        if (ret) {
            log::error("Memory registration failed for buffer queue element")("index", index)("error", fi_strerror(-ret));
            return Result::error_memory_registration_failed;
        }
        ++index;
    }

    log::info("All buffer queue elements registered successfully");
    return Result::success;
}

Result Rdma::cleanup_resources(context::Context& ctx)
{
    if (ep_ctx) {
        int err = libfabric_ep_ops.ep_destroy(&ep_ctx);
        if (err) {
            log::error("Failed to destroy RDMA endpoint")("error", fi_strerror(-err));
        }
        ep_ctx = nullptr;
    }

    // Clean up the buffer queue
    cleanup_queue();

    // Mark RDMA as uninitialized
    init = false;

    log::info("RDMA resources cleaned up successfully");
    return Result::success;
}

void Rdma::shutdown_rdma(context::Context& ctx)
{
    log::info("shutdown_rdma: Initiating RDMA shutdown");

    // Notify all threads waiting on the condition variable
    {
        log::debug("shutdown_rdma: Notifying all threads waiting on the condition variable");
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue_cv.notify_all();
    }

    // Cancel the `rdma_cq_thread` context
    log::info("shutdown_rdma: Cancelling rdma_cq_thread context");
    rdma_cq_thread_ctx.cancel();

    // Join the `rdma_cq_thread` if joinable
    if (handle_rdma_cq_thread.joinable()) {
        log::info("shutdown_rdma: Joining rdma_cq_thread");
        handle_rdma_cq_thread.join();
        log::info("shutdown_rdma: rdma_cq_thread joined successfully");
    } else {
        log::warn("shutdown_rdma: rdma_cq_thread is not joinable");
    }

    // Cancel the `process_buffers_thread` context
    log::info("shutdown_rdma: Cancelling process_buffers_thread context");
    process_buffers_thread_ctx.cancel();

    // Join the `process_buffers_thread` if joinable
    if (handle_process_buffers_thread.joinable()) {
        log::info("shutdown_rdma: Joining process_buffers_thread");
        handle_process_buffers_thread.join();
        log::info("shutdown_rdma: process_buffers_thread joined successfully");
    } else {
        log::warn("shutdown_rdma: process_buffers_thread is not joinable");
    }

    // Clean up resources if initialized
    if (init) {
        log::info("shutdown_rdma: Cleaning up RDMA resources");
        cleanup_resources(ctx);
    } else {
        log::warn("shutdown_rdma: RDMA not initialized, skipping resource cleanup");
    }

    // Set the RDMA state to closed
    set_state(ctx, State::closed);
    log::info("shutdown_rdma: RDMA shutdown completed successfully");
}

} // namespace connection

} // namespace mesh
