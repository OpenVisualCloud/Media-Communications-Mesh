#include "conn_rdma_tx.h"
#include <stdexcept>

namespace mesh
{

namespace connection
{

RdmaTx::RdmaTx() : Rdma() {}

RdmaTx::~RdmaTx()
{
    // Any specific cleanup for Tx can go here
}

Result RdmaTx::configure(context::Context& ctx, const mcm_conn_param& request,
                         const std::string& dev_port,
                         libfabric_ctx *& dev_handle)
{
    log::info("Configuring RdmaTx")("dev_port", dev_port)
             ("kind", "transmitter")("direction", "TX");
    return Rdma::configure(ctx, request, dev_port, dev_handle,
                           Kind::transmitter, direction::TX);
}

Result RdmaTx::handle_buffers(context::Context& ctx, void *buffer, size_t size)
{
    // Handle completion events for Tx
    int err = libfabric_ep_ops.ep_cq_read(ep_ctx, &buffer, RDMA_DEFAULT_TIMEOUT);
    if (err == -EAGAIN) {
        // No completion yet
        return Result::success;
    } else if (err) {
        log::error("Completion queue read failed")("error", fi_strerror(-err));
        return Result::error_general_failure;
    }

    // Example: Increment a counter for sent buffers
    log::info("Buffer successfully sent through completion queue")
             ("buffer_address", buffer);

    return Result::success;
}

Result RdmaTx::on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                          uint32_t& sent)
{
    log::info("Entering RdmaTx::on_receive")("incoming_size", sz);

    // Temporary variable for sent size
    uint32_t temp_sent = std::min(static_cast<uint32_t>(transfer_size), sz);
    if (temp_sent != transfer_size) {
        log::warn("Sent size differs from transfer size")(
                  "requested_size", temp_sent)("transfer_size", transfer_size);
    }

    // Check for available pre-allocated buffer
    if (buffers.empty() || !buffers[0]) {
        log::error("No pre-allocated buffer available for RDMA transmission");
        sent = 0;
        return set_result(Result::error_general_failure);
    }

    void *registered_buffer = buffers[0];
    log::info("Copying data to the registered buffer")
             ("buffer_address", registered_buffer)("size", temp_sent);

    // Copy data to the registered buffer
    std::memcpy(registered_buffer, ptr, temp_sent);

    // Print the contents of the registered buffer
    std::string buffer_contents(static_cast<char *>(registered_buffer),
                                temp_sent);
    log::debug("Buffer contents")("size", temp_sent)("data",
               buffer_contents.c_str());

    // Send the buffer using RDMA
    log::info("Sending buffer through RDMA")("size", temp_sent);
    int err = libfabric_ep_ops.ep_send_buf(ep_ctx, registered_buffer, temp_sent);
    if (err) {
        log::error("Failed to send buffer through RDMA")
                  ("error",fi_strerror(-err));
        sent = 0;
        return set_result(Result::error_general_failure);
    }

    // Update the sent variable after successful transmission
    sent = temp_sent;

    log::info("Buffer successfully sent through RDMA")("sent_size", sent);
    return set_result(Result::success);
}

} // namespace connection

} // namespace mesh
