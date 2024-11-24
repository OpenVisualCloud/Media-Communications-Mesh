#include "conn_rdma_rx.h"
#include <stdexcept>

namespace mesh {

namespace connection {

RdmaRx::RdmaRx() : Rdma() {}

RdmaRx::~RdmaRx()
{
    // Any specific cleanup for Rx can go here
}

Result RdmaRx::configure(context::Context& ctx, const mcm_conn_param& request,
                         const std::string& dev_port, libfabric_ctx *& dev_handle)
{
    return Rdma::configure(ctx, request, dev_port, dev_handle, Kind::receiver, direction::RX);
}

Result RdmaRx::start_threads(context::Context& ctx)
{
    try {
        handle_process_buffers_thread = std::jthread([this, &ctx]() {
            this->process_buffers_thread(_ctx);
        });
        handle_rdma_cq_thread = std::jthread([this, &ctx]() {
            this->rdma_cq_thread(_ctx);
        });
    } catch (const std::system_error& e) {
        log::fatal("Failed to start threads")("error", e.what());
        return Result::error_thread_creation_failed;
    }
    return Result::success;
}

// Thread to process available buffers in the ring buffer
void RdmaRx::process_buffers_thread(context::Context& ctx)
{
    while (!_ctx.cancelled()) {
        void* buf = nullptr;

        // Consume a buffer from the ring
        Result res = consume_from_ring(_ctx, &buf);
        if (res != Result::success) {
            if (_ctx.cancelled()) break; // Exit if shutdown is initiated
            log::warn("Failed to consume buffer from ring buffer")("result", static_cast<int>(res));
            continue; // Retry on non-critical errors
        }
        if (buf == nullptr) {
            log::error("Consumed buffer is null");
        }

        // Register the buffer for RDMA reception
        res = process_buffers(_ctx, buf, trx_sz);
        if (res != Result::success) {
            log::error("Failed to register buffer for RDMA reception")("buffer_address", buf);
            // Return the buffer to the ring in case of a failure
            if (add_to_ring(buf) != Result::success) {
                log::error("Failed to return buffer to ring after registration failure")("buffer_address", buf);
            }
            break; // Exit on critical errors
        }
    }
}

Result RdmaRx::process_buffers(context::Context& ctx, void *buf, size_t sz)
{
    int err = libfabric_ep_ops.ep_recv_buf(ep_ctx, buf, sz, buf);
    if (err) {
        log::error("Failed to pass empty buffer to RDMA to receive into")
        ("buffer_address",buf)("error", fi_strerror(-err));
        return Result::error_general_failure;
    }

    return Result::success;
}

// Thread to handle CQ events and pass buffers back to the ring buffer
void RdmaRx::rdma_cq_thread(context::Context& ctx)
{
    while (!_ctx.cancelled()) {
        void* buf = nullptr;

        // Wait for a CQ event
        Result res = handle_rdma_cq(_ctx, buf, trx_sz);
        if (res != Result::success) {
            if (_ctx.cancelled()) break; // Exit if shutdown is initiated
            log::error("handle_rdma_cq failed")("result", static_cast<int>(res));
        }
        if (buf == nullptr) {
            if (_ctx.cancelled()) break; // Exit if shutdown is initiated
            log::error("Completion queue provided a null buffer");
            continue; // Skip further processing
        }

        // Add the buffer back to the ring
        if (buf == nullptr) {
            log::error("Attempted to add a null buffer to the ring, skipping");
        } else {
            res = add_to_ring(buf);
            if (res != Result::success) {
                log::error("Failed to add buffer back to the ring buffer")("buffer_address", buf);
            }
        }
    }
}

Result RdmaRx::handle_rdma_cq(context::Context& ctx, void *buf, size_t sz)
{
    while (!_ctx.cancelled()) {
        int err = libfabric_ep_ops.ep_cq_read(ep_ctx, (void **)&buf, RDMA_DEFAULT_TIMEOUT);
        if (err == -EAGAIN) {
            continue;
        } else if (err) {
            log::error("Completion queue read failed")("buffer_address", buf)
                      ("error",fi_strerror(-err));
            return Result::error_general_failure;
        }
        if (buf == nullptr) {
            log::error("Completion queue read returned a null buffer");
            return Result::error_general_failure;
        }
        break;
    }

    Result res = transmit(_ctx, buf, trx_sz);
    if (res != Result::success && !ctx.cancelled()) {
        log::error("Failed to transmit buffer")("buffer_address", buf)("size", trx_sz);
        return res;
    }
    return Result::success;
}



} // namespace connection

} // namespace mesh
