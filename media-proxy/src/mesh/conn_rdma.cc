#include "conn_rdma.h"
#include "concurrency.h" // Include concurrency support
#include <cstring>       // For std::memset

namespace mesh
{

namespace connection
{

Rdma::Rdma()
    : ep_ctx(nullptr), initialized(false), transfer_size(0), mDevHandle(nullptr),
      thread_ctx(context::WithCancel(context::Background()))
{
    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
}

Rdma::~Rdma()
{
    if (initialized) {
        INFO("Rdma destructor called. Cleaning up resources...");
        cleanup_resources(thread_ctx);
    }
}

void Rdma::check_context_cancelled(context::Context &ctx)
{
    if (ctx.cancelled()) {
        INFO("%s, Rdma::check_context_cancelled: Context is cancelled.", __func__);
    } else {
        INFO("%s, Rdma::check_context_cancelled: Context is still active.", __func__);
    }
}

 //  check_context_cancelled(ctx); // Check and log context status

Result Rdma::configure(context::Context &ctx, const mcm_conn_param &request,
                       const std::string &dev_port, libfabric_ctx *&dev_handle, Kind kind,
                       direction dir)
{
 

    if (initialized) {
        ERROR("RDMA device is already initialized.");
        return Result::error_already_initialized;
    }

    int ret;
    Result res;

    // Initialize the RDMA device if not already initialized
    if (!dev_handle) {
        ret = rdma_init(&dev_handle);
        if (ret) {
            ERROR("Failed to initialize RDMA device.");
            return Result::error_initialization_failed;
        }
    }

    // Store the device handle
    this->mDevHandle = dev_handle;

    // Set transfer size
    this->transfer_size = request.payload_args.rdma_args.transfer_size;

    // Set Kind and direction
    _kind = kind;

    // Fill endpoint configuration
    std::memset(&ep_cfg, 0, sizeof(ep_cfg));

    ep_cfg.rdma_ctx = dev_handle;
    std::memcpy(&ep_cfg.remote_addr, &request.remote_addr, sizeof(request.remote_addr));
    std::memcpy(&ep_cfg.local_addr, &request.local_addr, sizeof(request.local_addr));
    ep_cfg.dir = dir;

    // Initialize the endpoint context
    ret = ep_init(&ep_ctx, &ep_cfg);
    if (ret) {
        ERROR("Failed to initialize RDMA endpoint context: %s", fi_strerror(-ret));
        return Result::error_initialization_failed;
    }

    // Allocate buffers
    res = allocate_buffer(1, transfer_size);
    if (res != Result::success) {
        ERROR("Failed to allocate buffers");
        set_state(ctx, State::closed);
        return res;
    }

    // Configure the RDMA endpoint, including the completion queue
    res = configure_endpoint(ctx);
    if (res != Result::success) {
        set_state(ctx, State::closed);
        return res;
    }

    // Mark as initialized and configured
    initialized = true;
    INFO("Setting state to State::configured.");
    set_state(ctx, State::configured);

    INFO("Exiting Rdma::configure successfully.");
    return Result::success;
}

Result Rdma::on_establish(context::Context &ctx)
{
    INFO("Entering Rdma::on_establish.");

    if (state() != State::establishing) {
        ERROR("Rdma::on_establish called in invalid state: %d. Expected State::establishing.", static_cast<int>(state()));
        return Result::error_wrong_state;
    }

    INFO("Attempting to start RDMA frame thread.");
    // Start the frame thread with the context
    Result res = start_thread(ctx);
    if (res != Result::success) {
        ERROR("Failed to start RDMA frame thread with result: %d.", static_cast<int>(res));
        INFO("Cleaning up resources due to frame thread failure.");
        cleanup_resources(ctx);
        INFO("Setting state to State::closed.");
        set_state(ctx, State::closed);
        return res;
    }

    INFO("RDMA frame thread started successfully.");
    INFO("Setting state to State::active.");
    set_state(ctx, State::active);

    INFO("Exiting Rdma::on_establish successfully.");
    return Result::success;
}


Result Rdma::on_shutdown(context::Context &ctx)
{
    if (state() == State::closed || state() == State::deleting) {
        return Result::success; // Already shut down
    }

    set_state(ctx, State::closing);

    if (initialized) {
        cleanup_resources(ctx);
    }
    set_state(ctx, State::closed);
    return Result::success;
}

Result Rdma::allocate_buffer(size_t buffer_count, size_t buffer_size)
{
    this->buffer_count = buffer_count;
    buffers.resize(buffer_count);

    for (size_t i = 0; i < buffer_count; ++i) {
        buffers[i] = std::malloc(buffer_size);
        if (!buffers[i]) {
            // Free previously allocated buffers
            for (size_t j = 0; j < i; ++j) {
                std::free(buffers[j]);
                buffers[j] = nullptr;
            }
            buffers.clear();
            return Result::error_out_of_memory;
        }
        std::memset(buffers[i], 0, buffer_size);
    }

    INFO("Allocated %zu buffers of size %zu bytes each", buffer_count, buffer_size);
    return Result::success;
}

Result Rdma::configure_endpoint(context::Context &ctx)
{
    if (!ep_ctx) {
        ERROR("Endpoint context is not initialized.");
        return Result::error_wrong_state;
    }

    for (auto &buffer : buffers) {
        if (!buffer) {
            ERROR("Buffer allocation failed");
            return Result::error_out_of_memory;
        }
        INFO("Registering buffer at address: %p with size: %zu", buffer, transfer_size);
        int ret = ep_reg_mr(ep_ctx, buffer, transfer_size);
        if (ret) {
            ERROR("Memory registration failed: %s", fi_strerror(-ret));
            return Result::error_memory_registration_failed;
        }
    }
    return Result::success;
}

Result Rdma::start_thread(context::Context &ctx)
{
    stop_thread = false;

    try {
        frame_thread_handle = new std::thread(&Rdma::frame_thread, this, std::ref(ctx));
    } catch (const std::exception &e) {
        ERROR("Failed to start frame thread: %s", e.what());
        return Result::error_thread_creation_failed;
    }

    return Result::success;
}

void Rdma::stop_thread_func()
{
    stop_thread = true;

    if (frame_thread_handle) {
        if (frame_thread_handle
                ->joinable()) { // Check joinable() before joining to avoid potential exceptions
            frame_thread_handle->join();
        }
        delete frame_thread_handle;
        frame_thread_handle = nullptr;
    }
}

void Rdma::frame_thread(context::Context &ctx)
{
    INFO("%s, RDMA frame thread started", __func__);

    int iteration = 0; // To track loop iterations for debugging

    while (!ctx.cancelled() && !stop_thread) {
   //     INFO("Frame thread iteration: %d", iteration);

        for (size_t i = 0; i < buffers.size(); ++i) {
   //         INFO("Processing buffer index: %zu", i);

            auto &buffer = buffers[i];
            if (!buffer) {
                INFO("Buffer index %zu is not allocated. Skipping.", i);
                continue; // Skip unallocated buffers
            }

            // Log buffer address and transfer size
    //        INFO("Buffer index %zu, address: %p, transfer size: %zu", i, buffer, transfer_size);

            // Call handle_buffers without throwing exceptions
            Result res = handle_buffers(ctx, buffer, transfer_size);
            if (res != Result::success) {
                ERROR("handle_buffers returned error: %d on buffer index %zu", static_cast<int>(res), i);
                stop_thread = true;
                break;
            } else {
    //            INFO("handle_buffers successfully processed buffer index %zu", i);
            }
        }

        iteration++; // Increment iteration count
    }

    INFO("%s, RDMA frame thread stopping. Context cancelled: %s, Stop thread: %s", __func__,
         ctx.cancelled() ? "true" : "false",
         stop_thread ? "true" : "false");

    INFO("%s, RDMA frame thread stopped", __func__);
}

Result Rdma::cleanup_resources(context::Context &ctx)
{
    INFO("Entering Rdma::cleanup_resources.");

    // Stop the frame thread gracefully
    if (frame_thread_handle) {
        INFO("Stopping RDMA frame thread...");
        stop_thread_func();
    }

    // Free allocated buffers
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i]) {
            INFO("Freeing buffer at index %zu, address: %p", i, buffers[i]);
            std::free(buffers[i]);
            buffers[i] = nullptr; // Avoid dangling pointer
        }
    }
    buffers.clear();
    INFO("All buffers cleared.");

    // Clean up RDMA resources
    if (ep_ctx) {
        int err = ep_destroy(&ep_ctx);
        if (err) {
            ERROR("Failed to destroy RDMA endpoint: %s", fi_strerror(-err));
            return Result::error_general_failure;
        }
        ep_ctx = nullptr;
    }

    initialized = false;

    INFO("RDMA resources cleaned up successfully.");
    return Result::success;
}

} // namespace connection

} // namespace mesh
