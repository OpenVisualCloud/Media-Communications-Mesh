#include "conn_rdma_tx.h"
#include <stdexcept>
#include <queue>

namespace mesh {

namespace connection {

RdmaTx::RdmaTx() : Rdma() {
    init_buf_available();
}

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
    rdma_cq_thread_ctx = context::WithCancel(ctx);
    
    try {
        handle_rdma_cq_thread = std::jthread([this]() {
            ////log::info("Starting rdma_cq_thread")(" ", kind_to_string(_kind));
            this->rdma_cq_thread(this->rdma_cq_thread_ctx);
            ////log::info("Exiting rdma_cq_thread")(" ", kind_to_string(_kind));
        });
    } catch (const std::system_error& e) {
        log::fatal("Failed to start threads")("error", e.what())(" ", kind_to_string(_kind));
        return Result::error_thread_creation_failed;
    }
    return Result::success;
}

void RdmaTx::rdma_cq_thread(context::Context& ctx)
{
    ////log::info("rdma_cq_thread started");

    constexpr uint32_t RETRY_INTERVAL_US = 10; // Retry interval of 100 us
    constexpr uint32_t TIMEOUT_US = 50000;      // Total timeout of 1 ms

    while (!ctx.cancelled()) {
        // Wait for the buffer to become available
        wait_buf_available();

        if (ctx.stop_token().stop_requested()) {
            ////log::info("rdma_cq_thread detected ctx.cancelled(), exiting...")(" ", kind_to_string(_kind));
            break;
        }

        struct fi_cq_entry cq_entries[CQ_BATCH_SIZE];

        uint32_t elapsed_time = 0;
        while (elapsed_time < TIMEOUT_US) {
            int ret = fi_cq_read(ep_ctx->cq_ctx.cq, cq_entries, CQ_BATCH_SIZE);

            if (ret > 0) {
                //log::error("CQ read success")("batch", ret)("after",elapsed_time / 1000.0)(" ", kind_to_string(_kind));
                for (int i = 0; i < ret; i++) {
                    void *buf = cq_entries[i].op_context;
                    if (buf == nullptr) {
                        log::error("Null buffer context, skipping...")(" ", kind_to_string(_kind));
                        continue;
                    }

                    // Replenish buffer
                    Result res = add_to_queue(buf);
                    if (res != Result::success) {
                        log::error("Failed to add buffer back to queue")("buffer_address", buf)(" ", kind_to_string(_kind));
                    }
                }
                break; // Exit retry loop as CQ events were successfully processed
            } else if (ret == -EAGAIN) {
                // No events, retry after a short wait
                std::this_thread::sleep_for(std::chrono::microseconds(RETRY_INTERVAL_US));
                elapsed_time += RETRY_INTERVAL_US;
            } else {
                // Handle errors
                log::error("CQ read failed")("error", fi_strerror(-ret))(" ", kind_to_string(_kind));
                break; // Exit retry loop on error
            }

            if (ctx.stop_token().stop_requested()) {
                ////log::info("rdma_cq_thread detected ctx.cancelled(), exiting...")(" ", kind_to_string(_kind));
                return;
            }
        }

        // Log if timeout occurred without processing any events
        if (elapsed_time >= TIMEOUT_US) {
            log::debug("CQ read timed out after retries")(" ", kind_to_string(_kind));
        }
    }

    ////log::info("Exiting rdma_cq_thread")(" ", kind_to_string(_kind));
}


Result RdmaTx::on_receive(context::Context& ctx, void* ptr, uint32_t sz, uint32_t& sent) {
    void* reg_buf = nullptr;
    constexpr uint32_t TIMEOUT_US = 500000; // 0.5-second timeout
    constexpr uint32_t RETRY_INTERVAL_US = 10; // Retry interval of 1000 us
    uint32_t elapsed_time = 0;

    // Attempt to consume a buffer from the queue with a timeout
    while (elapsed_time < TIMEOUT_US) {
        Result res = consume_from_queue(ctx, &reg_buf);
        if (res == Result::success) {
            if (reg_buf != nullptr) {
                break; // Successfully got a buffer
            } else {
                log::debug("Buffer is null, retrying...")(" ", kind_to_string(_kind));
            }
        } else if (res != Result::error_no_buffer) {
            // Log non-retryable errors and exit
            log::error("Failed to consume buffer from queue")("result", static_cast<int>(res))(" ", kind_to_string(_kind));
            sent = 0;
            return res;
        }

        // Wait before retrying
        std::this_thread::sleep_for(std::chrono::microseconds(RETRY_INTERVAL_US));
        elapsed_time += RETRY_INTERVAL_US;
    }

    // Check if we failed to get a buffer within the timeout
    if (reg_buf == nullptr) {
        log::error("Failed to consume buffer within timeout")("timeout_ms", TIMEOUT_US)(" ", kind_to_string(_kind));
        log::error("Total MB sent:")(" ", total_sent / 1048576.0);
        sent = 0;
        return Result::error_timeout;
    }

    uint32_t tmp_sent = std::min(static_cast<uint32_t>(trx_sz), sz);
    if (tmp_sent != trx_sz) {
        log::debug("Sent size differs from transfer size")("requested_size", tmp_sent)("trx_sz", trx_sz)(" ", kind_to_string(_kind));
    }

    std::memcpy(reg_buf, ptr, tmp_sent);

    // Transmit the buffer through RDMA
    //usleep(100);
    int err = libfabric_ep_ops.ep_send_buf(ep_ctx, reg_buf, tmp_sent);
    notify_buf_available();
    sent = tmp_sent;
    total_sent += tmp_sent;
    //log::fatal("Total MB sent:")(" ", total_sent / 1048576.0);

    if (err) {
        log::error("Failed to send buffer through RDMA")("error", fi_strerror(-err))(" ", kind_to_string(_kind));

        // Add the buffer back to the queue in case of failure
        add_to_queue(reg_buf);

        sent = 0;
        return Result::error_general_failure;
    }

    return Result::success;
}

} // namespace connection

} // namespace mesh
