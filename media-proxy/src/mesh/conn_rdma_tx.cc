#include "conn_rdma_tx.h"
#include <stdexcept>

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

// Thread to handle CQ events and pass buffers back to the ring buffer
void RdmaTx::rdma_cq_thread(context::Context& ctx)
{
    log::info("rdma_cq_thread started");

    while (!ctx.cancelled()) {
        void* buf = nullptr;

        // Wait for a CQ event or cancellation signal
        {
            std::unique_lock<std::mutex> lock(cq_mutex);

            bool wait_result = cq_cv.wait_for(lock, std::chrono::milliseconds(1000),
                                              [&]() { return event_ready || ctx.cancelled(); });
            if (ctx.cancelled()) {
                log::info("rdma_cq_thread detected ctx.cancelled(), exiting...");
                break;
            }
            if (!wait_result || !event_ready) {
                continue;
            }

            // Reset event_ready if it was signaled
            event_ready = false;
        }

        usleep(5);
        
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

        // Add the buffer back to the ring
        Result res = add_to_ring(buf);
        if (res != Result::success) {
            log::error("Failed to add buffer back to the ring buffer")("buffer_address", buf);
        } else {
            log::debug("Buffer successfully added back to ring buffer")("buffer_address", buf);
        }
    }

    log::info("Exiting rdma_cq_thread");
}

Result RdmaTx::handle_rdma_cq(context::Context& ctx, void *buf, size_t size)
{
    int err = libfabric_ep_ops.ep_cq_read(ep_ctx, &buf, RDMA_DEFAULT_TIMEOUT);
    if (err == -EAGAIN) {
        return Result::success;
    } else if (err) {
        log::error("Completion queue read failed")("error", fi_strerror(-err));
        return Result::error_general_failure;
    }
    return Result::success;
}

Result RdmaTx::on_receive(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent)
{
    // Lock the ring buffer
    void *reg_buf = nullptr;

    // Consume a buffer from the ring
    Result res = consume_from_ring(ctx, &reg_buf);
    if (res == Result::error_buffer_underflow) {
        log::error("No available buffer in the ring for RDMA transmission");
        sent = 0;
        return Result::error_buffer_underflow;
    } else if (res != Result::success) {
        log::error("Failed to consume buffer from ring")("result", static_cast<int>(res));
        sent = 0;
        return Result::error_general_failure;
    }
    if (reg_buf == nullptr) {
        log::error("Consumed buffer is null");
    }

    uint32_t tmp_sent = std::min(static_cast<uint32_t>(trx_sz), sz);
    if (tmp_sent != trx_sz) {
        log::warn("Sent size differs from transfer size")("requested_size", tmp_sent)("trx_sz",
                                                                                      trx_sz);
    }

    if (!reg_buf) {
        log::error("Consumed buffer from ring is null");
        sent = 0;
        return set_result(Result::error_general_failure);
    }

    // Copy data into the registered buffer
    std::memcpy(reg_buf, ptr, tmp_sent);

    // Transmit the buffer through RDMA
    int err = libfabric_ep_ops.ep_send_buf(ep_ctx, reg_buf, tmp_sent);
    if (err) {
        log::error("Failed to send buffer through RDMA")("error", fi_strerror(-err));

        // Add the buffer back to the ring in case of failure
        add_to_ring(reg_buf);

        sent = 0;
        return set_result(Result::error_general_failure);
    }

    // Successfully transmitted; update sent size
    sent = tmp_sent;
    notify_cq_event();

    return set_result(Result::success);
}

} // namespace connection

} // namespace mesh
