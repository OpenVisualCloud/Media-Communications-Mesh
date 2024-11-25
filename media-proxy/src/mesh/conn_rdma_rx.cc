#include "conn_rdma_rx.h"
#include <stdexcept>
#include <queue>

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

Result RdmaRx::start_threads(context::Context& ctx) {
    log::info("Initializing threads for RDMA RX");

    process_buffers_thread_ctx = context::WithCancel(ctx);
    rdma_cq_thread_ctx = context::WithCancel(ctx);

    try {
        handle_process_buffers_thread = std::jthread(
            [this]() { 
                log::info("Starting process_buffers_thread");
                this->process_buffers_thread(this->process_buffers_thread_ctx); 
                log::info("Exiting process_buffers_thread");
            }
        );

        handle_rdma_cq_thread = std::jthread(
            [this]() { 
                log::info("Starting rdma_cq_thread");
                this->rdma_cq_thread(this->rdma_cq_thread_ctx); 
                log::info("Exiting rdma_cq_thread");
            }
        );

    } catch (const std::system_error& e) {
        log::fatal("Failed to start threads")("error", e.what());
        return Result::error_thread_creation_failed;
    }
    return Result::success;
}

void RdmaRx::notify_cq_event() {
    std::lock_guard<std::mutex> lock(cq_mutex);
    event_ready = true;
    cq_cv.notify_one();
}

// Thread to process available buffers in the queue
void RdmaRx::process_buffers_thread(context::Context& ctx)
{
    log::info("process_buffers_thread started");

    while (!ctx.cancelled()) {
        log::debug("Attempting to consume buffers from the queue...");
        void* buf = nullptr;

        // Process all available buffers in the queue
        while (!ctx.cancelled()) {
            Result res = consume_from_queue(ctx, &buf);
            if (res == Result::success && buf != nullptr) {
                log::debug("Registering buffer for RDMA reception")("buffer_address", buf);

                // Inline process_buffers logic
                int err = libfabric_ep_ops.ep_recv_buf(ep_ctx, buf, trx_sz, buf);
                if (err) {
                    log::error("Failed to pass empty buffer to RDMA to receive into")
                        ("buffer_address", buf)("error", fi_strerror(-err));

                    // Attempt to return the buffer to the queue
                    add_to_queue(buf);
                    log::debug("Buffer returned to queue after RDMA registration failure")
                        ("buffer_address", buf);
                } else {
                    log::debug("Buffer successfully registered for RDMA reception")
                        ("buffer_address", buf)("buffer_size", trx_sz);
                }
            } else {
                log::warn("Failed to consume buffer from queue")
                    ("result", static_cast<int>(res))
                    ("buffer_null", buf == nullptr);
                if (buf == nullptr) {
                    log::error("consume_from_queue returned a null buffer despite success status");
                }
                break; // Exit the loop to avoid spinning on errors
            }
        }

        // Wait for the next event if the queue is empty
        {
            log::debug("Queue empty or buffers processed, waiting for next CQ event...");
            std::unique_lock<std::mutex> lock(cq_mutex);

            bool notified = cq_cv.wait(lock, ctx.stop_token(), [&]() { return event_ready; });

            if (!notified) {
                log::warn("CQ wait interrupted without an event. Context canceled?")("ctx_cancelled", ctx.cancelled());
            }

            if (ctx.cancelled()) {
                log::info("process_buffers_thread detected cancellation, exiting...");
                break;
            }

            // Reset event flag
            log::debug("CQ event detected, resetting event flag and resuming processing...");
            event_ready = false;
        }
    }

    log::info("Exiting process_buffers_thread");
}


// Thread to handle CQ events and pass buffers back to the queue
void RdmaRx::rdma_cq_thread(context::Context& ctx)
{
    log::info("rdma_cq_thread started");

    while (!ctx.cancelled()) {
        log::debug("Waiting for a CQ event...");
        void* buf = nullptr;

        // Completion queue read loop
        while (!ctx.cancelled()) {
            int err = libfabric_ep_ops.ep_cq_read(ep_ctx, (void**)&buf, RDMA_DEFAULT_TIMEOUT);
            if (err == -EAGAIN) {
                continue; // Try again
            } else if (err) {
                log::error("Completion queue read failed")("error", fi_strerror(-err));
                continue; // Handle failure gracefully
            }
            if (buf == nullptr) {
                log::error("Completion queue read returned a null buffer");
                continue;
            }
            break;
        }

        if (buf == nullptr) {
            log::error("No valid buffer retrieved from CQ");
            continue;
        }

        // Transmit buffer
        Result res = transmit(ctx, buf, trx_sz);
        if (res != Result::success) {
            log::error("Failed to transmit buffer")("buffer_address", buf)("size", trx_sz);
            continue;
        }

        log::info("Buffer successfully transmitted")("buffer_address", buf);

        // Add buffer back to the queue
        res = add_to_queue(buf);
        if (res != Result::success) {
            log::error("Failed to add buffer back to the queue")("buffer_address", buf);
        }

        notify_cq_event();
    }

    log::info("Exiting rdma_cq_thread");
}

} // namespace connection

} // namespace mesh
