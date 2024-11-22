#include "conn_rdma.h"

namespace mesh {

namespace connection {

Rdma::Rdma() : ep_ctx(nullptr), init(false), trx_sz(0), mDevHandle(nullptr)
{
    std::memset(&ep_cfg, 0, sizeof(ep_cfg));
}

Rdma::~Rdma()
{
    shutdown_rdma(_ctx);
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

    set_state(ctx, State::configured);
    return Result::success;
}

Result Rdma::on_establish(context::Context& ctx)
{
    _ctx = context::WithCancel(ctx);

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

    res = allocate_buffer(16, trx_sz);
    if (res != Result::success) {
        log::error("Failed to allocate buffers")("trx_sz", trx_sz);
        if (ep_ctx) {
            log::debug("Destroying endpoint due to buffer allocation failure");
            libfabric_ep_ops.ep_destroy(&ep_ctx);
        }
        set_state(ctx, State::closed);
        return res;
    }

    res = configure_endpoint(_ctx);
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

    try {
        frame_thread_handle = std::jthread(&Rdma::frame_thread, this);
    } catch (const std::system_error& e) {
        log::fatal("Failed to start frame thread")("error", e.what());
        return Result::error_thread_creation_failed;
    }
    if (res != Result::success) {
        log::error("Failed to start RDMA frame thread")("result", static_cast<int>(res));
        cleanup_resources(_ctx);
        set_state(ctx, State::closed);
        return res;
    }

    set_state(ctx, State::active);
    return Result::success;
}

Result Rdma::on_shutdown(context::Context& ctx)
{
    // Note: this is a blocking call
    shutdown_rdma(_ctx);

    return Result::success;
}

void Rdma::on_delete(context::Context& ctx)
{
    // Note: this is a blocking call
    on_shutdown(_ctx);
}

Result Rdma::allocate_buffer(size_t buf_cnt, size_t buffer_size)
{
    this->buf_cnt = buf_cnt;
    bufs.resize(buf_cnt);

    for (size_t i = 0; i < buf_cnt; ++i) {
        bufs[i] = std::malloc(buffer_size);
        if (!bufs[i]) {
            for (size_t j = 0; j < i; ++j) {
                std::free(bufs[j]);
                bufs[j] = nullptr;
            }
            bufs.clear();
            return Result::error_out_of_memory;
        }
    }
    return Result::success;
}

Result Rdma::configure_endpoint(context::Context& ctx)
{
    if (!ep_ctx) {
        log::error("Endpoint context is not initialized");
        return Result::error_wrong_state;
    }

    for (auto& buf : bufs) {
        if (!buf) {
            ERROR("Buffer allocation failed");
            return Result::error_out_of_memory;
        }
        int ret = libfabric_ep_ops.ep_reg_mr(ep_ctx, buf, trx_sz);
        if (ret) {
            log::error("Memory registration failed")("error", fi_strerror(-ret));
            return Result::error_memory_registration_failed;
        }
    }
    return Result::success;
}

void Rdma::frame_thread()
{
    while (!_ctx.cancelled()) {

        for (size_t i = 0; i < bufs.size(); ++i) {

            auto& buf = bufs[i];
            if (!buf) {
                log::error("Buffer index %zu is not allocated. Skipping.", i);
                continue; // Skip unallocated buffers
            }

            Result res = process_buffers(_ctx, buf, trx_sz);
            if (res != Result::success) {
                log::error("Error processing buffers")("index", i)("result", static_cast<int>(res));
                break;
            }

            res = handle_buffers(_ctx, buf, trx_sz);
            if (res != Result::success) {
                log::error("Error handling buffers")("index", i)("result", static_cast<int>(res));
                break;
            }
        }

    }

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

    for (size_t i = 0; i < bufs.size(); ++i) {
        if (bufs[i]) {
            std::free(bufs[i]);
            bufs[i] = nullptr;
        }
    }
    bufs.clear();
    init = false;
    return Result::success;
}

void Rdma::shutdown_rdma(context::Context& ctx)
{
    _ctx.cancel();

    if (frame_thread_handle.joinable())
        frame_thread_handle.join();

    if (init) {
        cleanup_resources(_ctx);
    }

    set_state(ctx, State::closed);
}

} // namespace connection

} // namespace mesh
