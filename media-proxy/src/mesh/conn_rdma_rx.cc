#include "conn_rdma_rx.h"
#include <stdexcept>

namespace mesh
{

namespace connection
{

RdmaRx::RdmaRx() : Rdma()
{
    log::info("RdmaRx instance created.");
}

RdmaRx::~RdmaRx()
{
    log::info("RdmaRx destructor called, cleaning up resources");
    // Any specific cleanup for Rx can go here
}

Result RdmaRx::configure(context::Context& ctx, const mcm_conn_param& request,
                         const std::string& dev_port,
                         libfabric_ctx *& dev_handle)
{
    log::info("Configuring RdmaRx")("dev_port", dev_port)("kind", "receiver")
             ("direction", "RX");
    return Rdma::configure(ctx, request, dev_port, dev_handle, Kind::receiver,
                           direction::RX);
}

Result RdmaRx::process_buffers(context::Context& ctx, void *buf, size_t sz)
{
    Result res = receive_data(ctx, buf, transfer_size);
    if (res != Result::success) {
        log::error("Failed to pass buffer to RDMA")("result", result2str(res));
    }

    // ?????????????????? shutdown_rdma(ctx);
    return res;
}

Result RdmaRx::handle_buffers(context::Context& ctx, void *buffer, size_t size)
{
    // INFO("Entering RdmaRx::handle_buffers.");
    // INFO("In handle_buffers current buffer address is: %p", buffer);

    while (!ctx.cancelled()) {

        // Try to read the completion queue with the provided buffer
        int err = libfabric_ep_ops.ep_cq_read(ep_ctx, (void **)&buffer, RDMA_DEFAULT_TIMEOUT);
        if (err == -EAGAIN) {
            // No completion event yet; keep looping
            //    INFO("No completion event yet for buffer: %p, retrying...",
            //    buffer);
            continue;
        } else if (err) {
            // Handle errors other than -EAGAIN
            log::error("Completion queue read failed")
                      ("buffer_address", buffer)("error", fi_strerror(-err));
            return Result::error_general_failure;
        }

        if (buffer == nullptr) {
            log::error("Completion queue read returned a null buffer");
            return Result::error_general_failure;
        }

        // Break the loop when a completion event is received
        log::info("Completion event received")("buffer_address", buffer);
        break;
    }

    // Process the completed buffer and forward to the emulated receiver
    log::info("Processing buffer for transmission")
             ("buffer_address", buffer)("size", transfer_size);
    Result res = transmit(ctx, buffer, transfer_size);
    if (res != Result::success && !ctx.cancelled()) {
        log::error("Failed to transmit buffer")("buffer_address",
                   buffer)("size", transfer_size);
        return res;
    }

    log::info("Successfully processed and forwarded buffer")
             ("buffer_address", buffer);

    return Result::success;
}

Result RdmaRx::receive_data(context::Context& ctx, void *buffer,
                            size_t buffer_size)
{
    log::info("Receiving data")("buffer_size", buffer_size)
             ("buffer_address", buffer);
    
    int err = libfabric_ep_ops.ep_recv_buf(ep_ctx, buffer, buffer_size, buffer);
    if (err) {
        log::error("Failed to receive data into buffer")("buffer_address", buffer)
                  ("error", fi_strerror(-err));
        return Result::error_general_failure;
    }

    return Result::success;
}

} // namespace connection

} // namespace mesh
