#include "conn_rdma_rx.h"
#include <stdexcept>
#include <queue>

namespace mesh {

namespace connection {

RdmaRx::RdmaRx() : Rdma() { init_buf_available(); }

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

    process_buffers_thread_ctx = context::WithCancel(ctx);
    rdma_cq_thread_ctx = context::WithCancel(ctx);

    try {
        handle_process_buffers_thread = std::jthread(
            [this]() { this->process_buffers_thread(this->process_buffers_thread_ctx); });
    } catch (const std::system_error& e) {
        log::fatal("Failed to start thread")("error", e.what());
        return Result::error_thread_creation_failed;
    }

    try {
        handle_rdma_cq_thread =
            std::jthread([this]() { this->rdma_cq_thread(this->rdma_cq_thread_ctx); });
    } catch (const std::system_error& e) {
        log::fatal("Failed to start thread")("error", e.what());
        return Result::error_thread_creation_failed;
    }

    return Result::success;
}

/**
 * @brief Handles the buffer processing logic for RDMA in a dedicated thread.
 * 
 * Continuously consumes available buffers from the queue and prepares them for RDMA reception
 * by passing them to the RDMA endpoint. If no buffers are available, the thread waits for 
 * notification of buffer availability. Ensures graceful handling of errors and context cancellation.
 * 
 * @param ctx The context for managing thread cancellation and operations.
 */
void RdmaRx::process_buffers_thread(context::Context& ctx)
{

    while (!ctx.cancelled()) {
        void *buf = nullptr;

        // Process all available buffers in the queue
        while (!ctx.cancelled()) {
            Result res = consume_from_queue(ctx, &buf);
            if (res == Result::success && buf != nullptr) {
                int err = libfabric_ep_ops.ep_recv_buf(ep_ctx, buf, trx_sz, buf);
                if (err) {
                    log::error("Failed to pass empty buffer to RDMA to receive into")
                              ("buffer_address", buf)("error", fi_strerror(-err))
                              (" ",kind_to_string(_kind));
                    add_to_queue(buf);
                }
            } else {
                break; // Exit the loop to avoid spinning on errors
            }
        }
        // Wait for the buffer to become available
        wait_buf_available();
    }
}

/**
 * @brief Handles the RDMA completion queue (CQ) events in a dedicated thread.
 * 
 * This function continuously monitors the CQ for completion events, processes
 * completed buffers, and returns them to the buffer queue for reuse. It ensures
 * efficient event handling by reading events in batches and sleeping briefly
 * when no events are available to avoid busy waiting.
 * 
 * Key Steps:
 * 1. Reads a batch of CQ entries.
 * 2. For each entry:
 *    - Processes the received buffer and sends it.
 *    - Adds the buffer back to the queue upon successful processing.
 *    - Logs errors for any issues during processing or queue operations.
 * 3. Handles `EAGAIN` errors by yielding CPU time to avoid busy looping.
 * 4. Logs and exits on other CQ read errors.
 * 
 * @param ctx The context for managing thread cancellation and operations.
 */
void RdmaRx::rdma_cq_thread(context::Context& ctx)
{
    constexpr int CQ_RETRY_DELAY_US = 100; // Retry delay for EAGAIN
    struct fi_cq_entry cq_entries[CQ_BATCH_SIZE]; // Array to hold batch of CQ entries

    while (!ctx.cancelled()) {
        // Read a batch of completion events
        int ret = fi_cq_read(ep_ctx->cq_ctx.cq, cq_entries, CQ_BATCH_SIZE);
        if (ret > 0) {
            for (int i = 0; i < ret; ++i) {
                void *buf = cq_entries[i].op_context;
                if (buf == nullptr) {
                    log::error("Null buffer context, skipping...")
                              ("batch_index",i)(" ", kind_to_string(_kind));
                    continue;
                }

                // Process the buffer (e.g., transmit or handle)
                Result res = transmit(ctx, buf, trx_sz);
                if (res != Result::success) {
                    log::error("Failed to transmit buffer")("buffer_address", buf)("size", trx_sz)
                              (" ", kind_to_string(_kind));
                    continue;
                }

                // Add the buffer back to the queue
                res = add_to_queue(buf);
                if (res == Result::success) {
                    // Notify that a buffer is available
                    notify_buf_available();
                } else {
                    log::error("Failed to add buffer back to the queue")("buffer_address", buf)
                              (" ", kind_to_string(_kind));
                }
            }
        } else if (ret == -EAGAIN) {
            // No events to process, yield CPU briefly to avoid busy looping
            std::this_thread::sleep_for(std::chrono::microseconds(CQ_RETRY_DELAY_US));
        } else {
            // Handle CQ read error
            log::error("CQ read failed")("error", fi_strerror(-ret));
            break;
        }
    }
}

} // namespace connection

} // namespace mesh
