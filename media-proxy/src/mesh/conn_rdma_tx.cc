#include "conn_rdma_tx.h"
#include <stdexcept>

namespace mesh {

namespace connection {

RdmaTx::RdmaTx() : Rdma() {}

RdmaTx::~RdmaTx()
{
    // Any specific cleanup for Tx can go here
}

Result RdmaTx::configure(context::Context &ctx, const mcm_conn_param &request,
                         const std::string &dev_port, libfabric_ctx *&dev_handle)
{
    return Rdma::configure(ctx, request, dev_port, dev_handle, Kind::transmitter, direction::TX);
}

Result RdmaTx::handle_buffers(context::Context &ctx, void *buffer, size_t size)
{
    if (ctx.cancelled()) {
        return Result::error_operation_cancelled;
    }
    // Handle completion events for Tx
    int err = ep_cq_read(ep_ctx, NULL, RDMA_DEFAULT_TIMEOUT);
    if (err == -EAGAIN) {
        // No completion yet
        return Result::success;
    } else if (err) {
        ERROR("Completion queue read failed: %s", fi_strerror(-err));
        return Result::error_general_failure;
    }

    // Example: Increment a counter for sent buffers
    INFO("Successfully sent buffer.");

    // Process the completion event
    return process_completion_event(buffer);
}

Result RdmaTx::process_completion_event(void* buf_ctx)
{
    // Process Tx-specific completion events
    INFO("Processed Tx buffer completion for context: %p", buf_ctx);

    // Implement actual processing logic here

    return Result::success;
}

Result RdmaTx::on_receive(context::Context &ctx, void *ptr, uint32_t sz, uint32_t &sent)
{
    INFO("Entering RdmaTx::on_receive.");
    
    if (stop_thread) {
        ERROR("RdmaTx is stopped; cannot process received data.");
        sent = 0;
        return set_result(Result::error_general_failure);
    }

    // Is data of allowed transfer size?
    sent = std::min(static_cast<uint32_t>(transfer_size), sz);
    if (sent != transfer_size) {
        ERROR("RdmaTx::on_receive - Sent size (%u) differs from transfer size (%zu).", sent, transfer_size);
    }

    // Get the buffer from the pre-allocated and registered buffers
    if (buffers.empty() || !buffers[0]) {
        ERROR("RdmaTx::on_receive - No pre-allocated buffer available.");
        sent = 0;
        return set_result(Result::error_general_failure);
    }

    void *registered_buffer = buffers[0];
    INFO("Copying data to the registered buffer for RDMA transmission.");

    // Copy data to the registered buffer
    std::memcpy(registered_buffer, ptr, sent);

    // Print the contents of the registered buffer
    std::string buffer_contents(static_cast<char*>(registered_buffer), sent);
    INFO("Buffer contents (size %u): %s", sent, buffer_contents.c_str());

    // Send the buffer using RDMA
    INFO("Sending buffer of size %u through RDMA.", sent);
    int err = ep_send_buf(ep_ctx, registered_buffer, sent);
    if (err) {
        ERROR("ep_send_buf failed with: %s", fi_strerror(-err));
        sent = 0;
        return set_result(Result::error_general_failure);
    }

    INFO("Buffer successfully sent through RDMA. Sent size: %u.", sent);
    return set_result(Result::success);
}

} // namespace connection

} // namespace mesh
