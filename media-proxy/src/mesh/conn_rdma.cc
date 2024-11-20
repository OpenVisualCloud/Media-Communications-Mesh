#include "conn_rdma.h"

namespace mesh
{

namespace connection
{

Rdma::Rdma()
    : ep_ctx(nullptr), initialized(false), transfer_size(0), mDevHandle(nullptr)
{
    std::memset(&ep_cfg, 0,
                sizeof(ep_cfg)); // Ensure ep_cfg is fully zero-initialized
}

Rdma::~Rdma() { shutdown_rdma(context::Background()); }

Result Rdma::configure(context::Context& ctx, const mcm_conn_param& request,
                       const std::string& dev_port, libfabric_ctx *& dev_handle,
                       Kind kind, direction dir)
{
    // Set transfer size
    this->transfer_size = request.payload_args.rdma_args.transfer_size;

    // Set Kind and direction
    _kind = kind;

    // Fill endpoint configuration
    std::memset(&ep_cfg, 0, sizeof(ep_cfg));

    std::memcpy(&ep_cfg.remote_addr, &request.remote_addr,
                sizeof(request.remote_addr));
    std::memcpy(&ep_cfg.local_addr, &request.local_addr,
                sizeof(request.local_addr));
    ep_cfg.dir = dir;

    set_state(ctx, State::configured);

    log::info("RDMA configured successfully")("state", "configured")
             ("transfer_size", transfer_size);
    return Result::success;
}

Result Rdma::on_establish(context::Context& ctx)
{
    log::info("RDMA on_establish started")("thread", "starting");

    if (initialized) {
        log::error("RDMA device is already initialized")
                  ("state","initialized");
        return Result::error_already_initialized;
    }

    int ret;
    Result res;

    // Initialize the RDMA device if not already initialized
    if (!mDevHandle) {
        ret = libfabric_dev_ops.rdma_init(&mDevHandle);
        if (ret) {
            log::error("Failed to initialize RDMA device")("ret", ret);
            return Result::error_initialization_failed;
        }
    }

    ep_cfg.rdma_ctx = mDevHandle;

    // Initialize the endpoint context
    ret = libfabric_ep_ops.ep_init(&ep_ctx, &ep_cfg);
    if (ret) {
        log::error("Failed to initialize RDMA endpoint context")
                  ("error", fi_strerror(-ret));
        return Result::error_initialization_failed;
    }

    // Allocate buffers
    res = allocate_buffer(1, transfer_size);
    if (res != Result::success) {
        log::error("Failed to allocate buffers")
                  ("transfer_size",transfer_size);
        set_state(ctx, State::closed);
        return res;
    }

    // Configure the RDMA endpoint, including the completion queue
    res = configure_endpoint(ctx);
    if (res != Result::success) {
        log::error("RDMA configing failed")("state", "closed");
        set_state(ctx, State::closed);
        return res;
    }

    // Mark as initialized and configured
    initialized = true;

    // Start the frame thread with the context
    res = start_thread(ctx);
    if (res != Result::success) {
        log::error("Failed to start RDMA frame thread")
                  ("result",static_cast<int>(res));
        log::info("Cleaning up resources due to frame thread failure.");
        cleanup_resources(ctx);
        log::info("Setting state to State::closed.");
        set_state(ctx, State::closed);
        return res;
    }

    log::info("RDMA on_establish completed successfully")("state", "active");
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
        // Initialize the buffer to zero to avoid uninitialized warnings
        // std::memset(buffers, 0, transfer_size);
    }

    log::info("Buffers allocated successfully")("buffer_count", buffer_count)(
        "buffer_size", buffer_size);
    return Result::success;
}

Result Rdma::configure_endpoint(context::Context& ctx)
{
    // INFO("Entering configure_endpoint.");

    if (!ep_ctx) {
        log::error("Endpoint context is not initialized");
        return Result::error_wrong_state;
    }

    for (auto& buffer : buffers) {
        if (!buffer) {
            ERROR("Buffer allocation failed");
            return Result::error_out_of_memory;
        }
        log::info("Registering buffer")("address", buffer)("size",
                                                           transfer_size);
        int ret = libfabric_ep_ops.ep_reg_mr(ep_ctx, buffer, transfer_size);
        if (ret) {
            log::error("Memory registration failed")("error",
                                                     fi_strerror(-ret));
            return Result::error_memory_registration_failed;
        }
    }
    return Result::success;
}

Result Rdma::start_thread(context::Context& ctx)
{
    thread_ctx = context::WithCancel(ctx);

    try {
        frame_thread_handle =
            std::thread(&Rdma::frame_thread, this, std::ref(thread_ctx));
    } catch (const std::system_error& e) {
        log::fatal("Failed to start frame thread")("error", e.what());
        return Result::error_thread_creation_failed;
    }
    log::info("Frame thread started successfully");
    return Result::success;
}

void Rdma::frame_thread(context::Context& ctx)
{
    log::info("RDMA frame_thread started");

    int iteration = 0; // To track loop iterations for debugging

    while (!ctx.cancelled()) {

        for (size_t i = 0; i < buffers.size(); ++i) {

            auto& buf = buffers[i];
            if (!buf) {
                log::error("Buffer index %zu is not allocated. Skipping.", i);
                continue; // Skip unallocated buffers
            }

            Result res = process_buffers(ctx, buf, transfer_size);
            if (res != Result::success) {
                log::error("Error processing buffers")("index", i)(
                           "result", static_cast<int>(res));
                // stop_thread = true;
                break;
            }

            res = handle_buffers(ctx, buf, transfer_size);
            if (res != Result::success) {
                log::error("Error handling buffers")("index", i)("result", static_cast<int>(res));
                // stop_thread = true;
                break;
            }
        }

        iteration++; // Increment iteration count
    }

    log::info("RDMA frame thread cancelled")("thread_name", __func__);
}

Result Rdma::cleanup_resources(context::Context& ctx)
{
    log::info("Cleaning up RDMA resources");

    // Clean up RDMA resources
    if (ep_ctx) {
        log::info("Destroying RDMA endpoint...");
        int err = libfabric_ep_ops.ep_destroy(&ep_ctx);
        if (err) {
            log::error("Failed to destroy RDMA endpoint")("error", fi_strerror(-err));
        } else {
            log::info("RDMA endpoint destroyed successfully");
        }
        ep_ctx = nullptr;
    } else {
        log::info("No RDMA endpoint to destroy. Skipping endpoint cleanup.");
    }

    // Free allocated buffers
    log::info("Starting to free allocated buffers")("total_buffers", buffers.size());
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i]) {
            log::info("Freeing buffer")("index", i)("address", buffers[i]);
            std::free(buffers[i]);
            buffers[i] = nullptr; // Avoid dangling pointer
            log::info("Buffer freed successfully")("index", i);
        } else {
            log::warn("Buffer already null, skipping")("index", i);
        }
    }
    buffers.clear();
    initialized = false;
    log::info("RDMA resources cleaned up successfully");
    return Result::success;
}

void Rdma::shutdown_rdma(context::Context& ctx)
{
    log::debug("Shutting down RDMA");

    thread_ctx.cancel();
    // ctx.cancel();

    if (frame_thread_handle.joinable())
        frame_thread_handle.join();

    if (initialized) {
        log::info("Rdma cleanup_resources called. Cleaning up resources...");
        cleanup_resources(ctx);
    }

    set_state(ctx, State::closed);
    log::info("RDMA shutdown completed");
}

} // namespace connection

} // namespace mesh
