#include "conn_rdma_tx.h"
#include <stdexcept>
#include <queue>

namespace mesh {

namespace connection {

RdmaTx::RdmaTx() : Rdma() {}

RdmaTx::~RdmaTx()
{
    // Any specific cleanup for Tx can go here
}

Result RdmaTx::configure(context::Context& ctx, const mcm_conn_param& request,
                         const std::string& dev_port, libfabric_ctx *& dev_handle)
{
    return Rdma::configure(ctx, request, dev_port, dev_handle, Kind::transmitter, direction::TX);
}

Result RdmaTx::start_threads(context::Context& ctx)
{
    try {
        handle_rdma_cq_thread = std::jthread([this, &ctx]() { this->rdma_cq_thread(ctx); });
    } catch (const std::system_error& e) {
        log::fatal("Failed to start threads")("error", e.what());
        return Result::error_thread_creation_failed;
    }
    return Result::success;
}

void RdmaTx::notify_cq_event() {
    std::lock_guard<std::mutex> lock(cq_mutex);
    event_ready = true;
    cq_cv.notify_one();
}

// Thread to handle CQ events and pass buffers back to the queue
void RdmaTx::rdma_cq_thread(context::Context& ctx)
{
    log::info("rdma_cq_thread started");

    while (!ctx.stop_token().stop_requested()) {
        void* buf = nullptr;

        // Wait for a CQ event or cancellation signal
        {
            std::unique_lock<std::mutex> lock(cq_mutex);

            cq_cv.wait(lock, ctx.stop_token(), [&]() { return event_ready; });

            if (ctx.stop_token().stop_requested()) {
                log::info("rdma_cq_thread detected ctx.cancelled(), exiting...");
                break;
            }

            // Reset event_ready if it was signaled
            event_ready = false;
        }

        // Wait for CQ events
        struct fi_cq_entry cq_entry;
        int ret = fi_cq_read(ep_ctx->cq_ctx.cq, &cq_entry, 1);
        if (ret > 0) {
            buf = cq_entry.op_context; // Retrieve the buffer address
        } else if (ret == -EAGAIN) {
            log::debug("No new CQ event, retrying...");
            continue;
        } else {
            log::error("Completion queue read failed")("error", fi_strerror(-ret));
            continue;
        }

        if (buf == nullptr) {
            log::error("Completion queue provided a null buffer, skipping...");
            continue;
        }

        // Add the buffer back to the queue
        Result res = add_to_queue(buf);
        if (res != Result::success) {
            log::error("Failed to add buffer back to the queue")("buffer_address", buf);
        } else {
            log::debug("Buffer successfully added back to queue")("buffer_address", buf);
        }
    }

    log::info("Exiting rdma_cq_thread");
}

Result RdmaTx::on_receive(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent)
{
    void* reg_buf = nullptr;

    // Consume a buffer from the queue
    Result res = consume_from_queue(ctx, &reg_buf);
    if (res != Result::success) {
        log::error("Failed to consume buffer from queue")("result", static_cast<int>(res));
        sent = 0;
        return res;
    }

    if (reg_buf == nullptr) {
        log::error("Consumed buffer is null");
        sent = 0;
        return Result::error_general_failure;
    }

    uint32_t tmp_sent = std::min(static_cast<uint32_t>(trx_sz), sz);
    if (tmp_sent != trx_sz) {
        log::warn("Sent size differs from transfer size")("requested_size", tmp_sent)("trx_sz", trx_sz);
    }

    std::memcpy(reg_buf, ptr, tmp_sent);

    // Transmit the buffer through RDMA
    int err = libfabric_ep_ops.ep_send_buf(ep_ctx, reg_buf, tmp_sent);
    if (err) {
        log::error("Failed to send buffer through RDMA")("error", fi_strerror(-err));

        // Add the buffer back to the queue in case of failure
        add_to_queue(reg_buf);

        sent = 0;
        return Result::error_general_failure;
    }

    sent = tmp_sent;
    notify_cq_event();

    return Result::success;
}

} // namespace connection

} // namespace mesh
