#include "conn_rdma_rx.h"
#include <stdexcept>

namespace mesh {

namespace connection {

RdmaRx::RdmaRx() : Rdma() {}

RdmaRx::~RdmaRx()
{
    // Any specific cleanup for Rx can go here
}

Result RdmaRx::configure(context::Context &ctx, const mcm_conn_param &request, 
                         const std::string &dev_port, libfabric_ctx *&dev_handle) {
    return Rdma::configure(ctx, request, dev_port, dev_handle, Kind::receiver, direction::RX);
}

void RdmaRx::frame_thread(context::Context &ctx)
{
    INFO("RdmaRx frame thread started.");

    while (!ctx.cancelled() && !stop_thread) {
        for (size_t i = 0; i < buffers.size(); ++i) {
            auto &buffer = buffers[i];
            if (!buffer) {
                continue;
            }
        
        INFO("in frame_thread current buffer address is: %p", buffer);

            INFO("Passing empty buffer to RDMA for receiving data: %p", buffer);
            Result res = receive_data(ctx, buffer, transfer_size);
            if (res != Result::success) {
                ERROR("Failed to pass buffer to RDMA: %s", result2str(res));
                stop_thread = true;
                break;
            }
        
        //    INFO("Handling received buffers.");
            res = handle_buffers(ctx, buffer, transfer_size);
            if (res != Result::success) {
                ERROR("handle_buffers returned error: %s", result2str(res));
                stop_thread = true;
                break;
            }
        }
    }

    INFO("RdmaRx frame thread stopped.");
}

Result RdmaRx::handle_buffers(context::Context &ctx, void *buffer, size_t size)
{
   // INFO("Entering RdmaRx::handle_buffers.");
    INFO("In handle_buffers current buffer address is: %p", buffer);
    // Check if the context is cancelled
    if (ctx.cancelled()) {
        INFO("Context is cancelled. Exiting handle_buffers.");
        return Result::error_operation_cancelled;
    }

    while (true) {
        // Try to read the completion queue with the provided buffer
        int err = ep_cq_read(ep_ctx, (void **)&buffer, RDMA_DEFAULT_TIMEOUT);
        if (err == -EAGAIN) {
            // No completion event yet; keep looping
        //    INFO("No completion event yet for buffer: %p, retrying...", buffer);
            continue;
        } else if (err) {
            // Handle errors other than -EAGAIN
            ERROR("Completion queue read failed for buffer %p: %s", buffer, fi_strerror(-err));
            return Result::error_general_failure;
        }

        // Break the loop when a completion event is received
        INFO("Completion event received for buffer: %p", buffer);
        break;
    }

    // Process the completed buffer and forward to the emulated receiver
    INFO("Processing buffer: %p, size: %zu", buffer, transfer_size);
    Result res = transmit(ctx, buffer, transfer_size);
    if (res != Result::success) {
        ERROR("Failed to transmit buffer: %p, size: %zu", buffer, transfer_size);
        return res;
    }
    
    INFO("Successfully processed and forwarded buffer: %p", buffer);

    return Result::success;
}

Result RdmaRx::process_completion_event(void* buf_ctx)
{
    // Process Rx-specific completion events
    INFO("Processed Rx buffer completion for context: %p", buf_ctx);

    // Implement actual processing logic here

    return Result::success;
}

Result RdmaRx::receive_data(context::Context& ctx, void* buffer, size_t buffer_size)
{
    // INFO("RdmaRx::receive_data called with buffer size: %zu", buffer_size);
    INFO("In receive_data current buffer address is: %p", buffer);

    if (state() != State::active) {
        ERROR("RdmaRx is not in active state.");
        return Result::error_wrong_state;
    }

    int err = ep_recv_buf(ep_ctx, buffer, buffer_size, buffer);
    if (err) {
        ERROR("ep_recv_buf failed for buffer %p: %s", buffer, fi_strerror(-err));
        return Result::error_general_failure;
    }

   // INFO("Data successfully received into buffer.");
    return Result::success;
}

} // namespace connection

} // namespace mesh
