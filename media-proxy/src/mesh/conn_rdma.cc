#include "conn_rdma.h"


namespace mesh {

namespace connection {

Rdma::Rdma() : ep_ctx(nullptr), init(false), trx_sz(0), mDevHandle(nullptr)
{
    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
}

Rdma::~Rdma() { shutdown_rdma(context::Background()); }

Result Rdma::init_ring_with_elements(size_t capacity, size_t trx_sz) {
    std::lock_guard<std::mutex> lock(ring_buffer.mtx);

    // Check if already initialized
    if (!ring_buffer.buf.empty()) {
        log::error("Ring buffer already initialized");
        return Result::error_already_initialized;
    }

    // Resize the ring buffer
    ring_buffer.buf.resize(capacity, nullptr);
    ring_buffer.capacity = capacity;

    // Allocate memory for each slot using calloc for zero-initialization
    for (size_t i = 0; i < capacity; ++i) {
        ring_buffer.buf[i] = std::calloc(1, trx_sz);
        if (!ring_buffer.buf[i]) {
            log::error("Failed to allocate memory for ring buffer element")("index", i)("trx_sz", trx_sz);
            cleanup_ring(); // Free already allocated memory
            return Result::error_out_of_memory;
        }
    }

    // Pre-fill the ring buffer: all buffers are ready for consumption
    ring_buffer.head = capacity; // All slots are filled
    ring_buffer.tail = 0;        // Start consuming from the first slot

    log::info("Ring buffer pre-filled with elements")("capacity", capacity)("element_size", trx_sz);
    return Result::success;
}

Result Rdma::add_to_ring(void* element) {
    if (!element) {
        log::error("Cannot add a nullptr to the ring buffer");
        return Result::error_bad_argument;
    }

    std::unique_lock<std::mutex> lock(ring_buffer.mtx);

    size_t next_head = (ring_buffer.head + 1) % ring_buffer.capacity;
    if (next_head == ring_buffer.tail) {
        log::error("Ring buffer overflow: unable to add element");
        return Result::error_buffer_overflow;
    }

    ring_buffer.buf[ring_buffer.head] = element;
    ring_buffer.head = next_head;

    log::debug("Element added to ring buffer")("head", ring_buffer.head)("tail", ring_buffer.tail);

    return Result::success;
}

Result Rdma::consume_from_ring(context::Context& ctx, void** element) {
    log::debug("consume_from_ring: Attempting to acquire buffer");

    if (!(*element)) {
        log::error("Output parameter for consume_from_ring is null");
        return Result::error_bad_argument;
    }

    if (ctx.cancelled()) {
        log::info("consume_from_ring: Context canceled, exiting wait");
        return Result::error_operation_cancelled;
    }

    if (ring_buffer.buf.empty() || ring_buffer.capacity == 0) {
        log::error("Ring buffer not initialized or empty");
        return Result::error_general_failure;
    }

    *element = ring_buffer.buf[ring_buffer.tail+1];
    if (*element == nullptr) {
        log::error("consume_from_ring: Null buffer encountered");
        return Result::error_general_failure;
    }

    ring_buffer.buf[ring_buffer.tail+1] = nullptr;
    ring_buffer.tail = (ring_buffer.tail + 1) % ring_buffer.capacity;

    log::debug("consume_from_ring: Buffer consumed")("tail", ring_buffer.tail);
    return Result::success;
}

void Rdma::cleanup_ring() {
    std::lock_guard<std::mutex> lock(ring_buffer.mtx);

    for (size_t i = 0; i < ring_buffer.capacity; ++i) {
        if (ring_buffer.buf[i]) {
            std::free(ring_buffer.buf[i]);
            ring_buffer.buf[i] = nullptr; // Clear slot after deallocation
            log::debug("Deallocated buffer at index")("index", i);
        }
    }

    ring_buffer.buf.clear();
    ring_buffer.capacity = 0;
    ring_buffer.head = 0;
    ring_buffer.tail = 0;

    log::info("Ring buffer cleaned up successfully");
}

Result Rdma::configure(context::Context& ctx, const mcm_conn_param& request,
                       const std::string& dev_port, libfabric_ctx *& dev_handle, Kind kind,
                       direction dir)
{
    this->trx_sz = request.payload_args.rdma_args.transfer_size;

    _kind = kind;

    std::memset(&ep_cfg, 0, sizeof(ep_cfg));

    std::memcpy(&ep_cfg.remote_addr, &request.remote_addr, sizeof(request.remote_addr));
    std::memcpy(&ep_cfg.local_addr, &request.local_addr, sizeof(request.local_addr));
    ep_cfg.dir = dir;

    mDevHandle = dev_handle;

    set_state(ctx, State::configured);
    return Result::success;
}

Result Rdma::on_establish(context::Context& ctx)
{
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

    res = init_ring_with_elements(16, trx_sz); // Assuming 16 slots in the ring buffer
    if (res != Result::success) {
        log::error("Failed to initialize ring buffer")("trx_sz", trx_sz);
        if (ep_ctx) {
            log::debug("Destroying endpoint due to ring buffer initialization failure");
            libfabric_ep_ops.ep_destroy(&ep_ctx);
        }
        set_state(ctx, State::closed);
        return res;
    }

    log::debug("Ring buffer initialized")("capacity", ring_buffer.capacity);
    for (size_t i = 0; i < ring_buffer.capacity; ++i) {
        log::debug("Ring buffer element at index")("index", i)("value", ring_buffer.buf[i]);
    }

    // Configure RDMA endpoint
    res = configure_endpoint(ctx);
    if (res != Result::success) {
        log::error("RDMA configuring failed")("state", "closed");
        if (ep_ctx) {
            log::debug("Destroying endpoint due to configuration failure");
            libfabric_ep_ops.ep_destroy(&ep_ctx);
        }
        cleanup_ring(); // Clean up ring buffer in case of failure
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

    std::lock_guard<std::mutex> lock(ring_buffer.mtx);

    for (size_t i = 0; i < ring_buffer.capacity; ++i) {
        if (!ring_buffer.buf[i]) {
            log::error("Ring buffer element is not allocated")("index", i);
            return Result::error_out_of_memory;
        }

        int ret = libfabric_ep_ops.ep_reg_mr(ep_ctx, ring_buffer.buf[i], trx_sz);
        if (ret) {
            log::error("Memory registration failed for ring buffer element")("index", i)("error", fi_strerror(-ret));
            return Result::error_memory_registration_failed;
        }
    }

    log::info("All ring buffer elements registered successfully");
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

    // Clean up the ring buffer
    cleanup_ring();

    // Mark RDMA as uninitialized
    init = false;

    log::info("RDMA resources cleaned up successfully");
    return Result::success;
}

void Rdma::shutdown_rdma(context::Context& ctx)
{
    log::info("shutdown_rdma: Initiating RDMA shutdown");

    // Notify all waiting threads in consume_from_ring
    {
        log::debug("shutdown_rdma: Notifying all threads waiting on condition variable");
        std::lock_guard<std::mutex> lock(ring_buffer.mtx);
        ring_buffer.cv.notify_one();
    }

    // Cancel the rdma_cq_thread context
    log::info("shutdown_rdma: Cancelling rdma_cq_thread context");
    rdma_cq_thread_ctx.cancel();

    // Join the rdma_cq_thread if joinable
    if (handle_rdma_cq_thread.joinable()) {
        log::info("shutdown_rdma: Joining rdma_cq_thread");
        handle_rdma_cq_thread.join();
        log::info("shutdown_rdma: rdma_cq_thread joined successfully");
    } else {
        log::warn("shutdown_rdma: rdma_cq_thread is not joinable");
    }

    // Cancel the process_buffers_thread context
    log::info("shutdown_rdma: Cancelling process_buffers_thread context");
    process_buffers_thread_ctx.cancel();

    // Join the process_buffers_thread if joinable
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
