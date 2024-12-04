#include <queue>
#include "conn_rdma.h"

namespace mesh {

namespace connection {

Rdma::Rdma() : ep_ctx(nullptr), init(false), trx_sz(0), mDevHandle(nullptr)
{
    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
}

Rdma::~Rdma() { shutdown_rdma(context::Background()); }

std::string Rdma::kind_to_string(Kind kind)
{
    switch (kind) {
    case Kind::undefined:
        return "undefined";
    case Kind::transmitter:
        return "transmitter";
    case Kind::receiver:
        return "receiver";
    default:
        return "unknown";
    }
}

void Rdma::notify_cq_event()
{
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
        return Result::error_bad_argument;
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
        return Result::error_operation_cancelled;
    }

    if (buffer_queue.empty()) {
        return Result::error_no_buffer;
    }

    *element = buffer_queue.front();
    buffer_queue.pop();

    return Result::success;
}

// Initialize the buffer queue with pre-allocated buffers
Result Rdma::init_queue_with_elements(size_t capacity, size_t trx_sz)
{
    if (trx_sz == 0 || trx_sz > MAX_BUFFER_SIZE) {
        log::error("Invalid transfer size provided for buffer allocation")("trx_sz", trx_sz);
        return Result::error_bad_argument;
    }

    std::lock_guard<std::mutex> lock(queue_mutex);

    // Check if already initialized
    if (!buffer_queue.empty()) {
        log::error("Buffer queue already initialized");
        return Result::error_already_initialized;
    }

    // Calculate total memory size needed
    size_t aligned_trx_sz =
        ((trx_sz + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE; // Align to page size
    size_t total_size = capacity * aligned_trx_sz;

    // Allocate a single contiguous memory block
    void *memory_block = std::aligned_alloc(PAGE_SIZE, total_size);
    if (!memory_block) {
        log::error("Failed to allocate a single memory block")("total_size", total_size);
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
    buffer_block =
        memory_block; // Assume buffer_block is a member variable to hold the base pointer

    log::info("Buffer queue initialized with elements")("capacity", capacity)("element_size",
              trx_sz);
    return Result::success;
}

// Cleanup the buffer queue
void Rdma::cleanup_queue()
{
    if (buffer_block) {
        std::free(buffer_block); // Free the entire memory block
        buffer_block = nullptr;
    }
    while (!buffer_queue.empty()) {
        buffer_queue.pop();
    }
}

Result Rdma::configure(context::Context& ctx, const mcm_conn_param& request,
                       const std::string& dev_port, libfabric_ctx *& dev_handle, Kind kind,
                       direction dir)
{

    if ((kind == Kind::receiver && dir != direction::RX) ||
        (kind == Kind::transmitter && dir != direction::TX)) {
        return Result::error_bad_argument;
    }

    trx_sz = request.payload_args.rdma_args.transfer_size;
    _kind = kind;

    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
    std::memcpy(&ep_cfg.remote_addr, &request.remote_addr, sizeof(request.remote_addr));
    std::memcpy(&ep_cfg.local_addr, &request.local_addr, sizeof(request.local_addr));
    ep_cfg.dir = dir;

    mDevHandle = dev_handle;

    queue_size = request.payload_args.rdma_args.queue_size;

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

    res = init_queue_with_elements(queue_size, trx_sz);
    if (res != Result::success) {
        log::error("Failed to initialize buffer queue")("trx_sz", trx_sz);
        if (ep_ctx) {
            libfabric_ep_ops.ep_destroy(&ep_ctx);
        }
        set_state(ctx, State::closed);
        return res;
    }

    // Configure RDMA endpoint
    res = configure_endpoint(ctx);
    if (res != Result::success) {
        log::error("RDMA configuring failed")("state", "closed");
        if (ep_ctx) {
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

    if (!buffer_block) {
        log::error("Memory block for buffer queue is not allocated");
        return Result::error_out_of_memory;
    }

    // Calculate the total size of the memory block
    size_t total_size = buffer_queue.size() * ((trx_sz + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE);

    // Register the entire memory block with the RDMA endpoint
    int ret = libfabric_ep_ops.ep_reg_mr(ep_ctx, buffer_block, total_size);
    if (ret) {
        log::error("Memory registration failed for the buffer block")("error", fi_strerror(-ret));
        return Result::error_memory_registration_failed;
    }

    log::info("Memory block registered successfully")("total_size", total_size);
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

    return Result::success;
}

void Rdma::shutdown_rdma(context::Context& ctx)
{
    // Cancel `rdma_cq_thread` context
    rdma_cq_thread_ctx.cancel();
    // Ensure buffer-related threads are unblocked
    notify_buf_available();

    // Check if `rdma_cq_thread` has stopped
    if (handle_rdma_cq_thread.joinable()) {
        handle_rdma_cq_thread.join();
    } else {
        log::debug("shutdown_rdma: rdma_cq_thread is not joinable");
    }

    // Cancel `process_buffers_thread` context
    process_buffers_thread_ctx.cancel();
    notify_buf_available();

    // Join the `process_buffers_thread`
    if (handle_process_buffers_thread.joinable()) {
        handle_process_buffers_thread.join();
    }

    // Clean up RDMA resources if initialized
    if (init) {
        cleanup_resources(ctx);
    } else {
        log::debug("shutdown_rdma: RDMA not initialized, skipping resource cleanup");
    }

    set_state(ctx, State::closed);
}

} // namespace connection

} // namespace mesh
