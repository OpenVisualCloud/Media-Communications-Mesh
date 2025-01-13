#include "conn_rdma_tx.h"
#include <stdexcept>
#include <queue>

namespace mesh::connection {

RdmaTx::RdmaTx() : Rdma() {
    _kind = Kind::transmitter; // Set the Kind in the constructor
}

RdmaTx::~RdmaTx()
{
    // Any specific cleanup for Tx can go here
}

Result RdmaTx::configure(context::Context& ctx, const mcm_conn_param& request,
                         libfabric_ctx *& dev_handle)
{
    log::debug("RdmaTx configure")("local_ip", request.local_addr.ip)
                                  ("local_port", request.local_addr.port)
                                  ("remote_ip", request.remote_addr.ip)
                                  ("remote_port", request.remote_addr.port);

    return Rdma::configure(ctx, request, dev_handle);
}

Result RdmaTx::start_threads(context::Context& ctx)
{
    rdma_cq_thread_ctx = context::WithCancel(ctx);

    try {
        handle_rdma_cq_thread =
            std::jthread([this]() { this->rdma_cq_thread(this->rdma_cq_thread_ctx); });
    } catch (const std::system_error& e) {
        log::error("RDMA tx failed to start threads")("error", e.what())
                  ("kind", kind2str(_kind));
        return Result::error_thread_creation_failed;
    }
    return Result::success;
}

/**
 * @brief Monitors the RDMA completion queue (CQ) for send completions and manages buffer recycling.
 * 
 * This function runs in a dedicated thread to handle send completion events from the RDMA CQ.
 * It waits for buffer availability notifications, processes CQ events, and replenishes buffers
 * to the queue for reuse. Implements a retry mechanism with a timeout to handle transient CQ read issues.
 * 
 * @param ctx The context for managing thread cancellation and stop requests.
 */

void RdmaTx::rdma_cq_thread(context::Context& ctx)
{
    constexpr uint32_t RETRY_INTERVAL_US = 100; // Retry interval of 100 us
    constexpr uint32_t TIMEOUT_US = 1000000;       // Total timeout of 1s

    while (!ctx.cancelled()) {
        // Wait for the buffer to become available after successful send
        wait_buf_available();

        struct fi_cq_entry cq_entries[CQ_BATCH_SIZE];

        uint32_t elapsed_time = 0;
        while (elapsed_time < TIMEOUT_US) {
            int ret = fi_cq_read(ep_ctx->cq_ctx.cq, cq_entries, CQ_BATCH_SIZE);

            if (ret > 0) {
                for (int i = 0; i < ret; i++) {
                    void *buf = cq_entries[i].op_context;
                    if (buf == nullptr) {
                        log::error("RDMA tx null buffer context, skipping...")
                                  ("kind", kind2str(_kind));
                        continue;
                    }

                    // Replenish buffer
                    Result res = add_to_queue(buf);
                    if (res != Result::success) {
                        log::error("RDMA tx failed to add buffer back to queue")
                                  ("buffer_address", buf)("kind", kind2str(_kind));
                    }
                }
                break; // Exit retry loop as CQ events were successfully processed
            } else if (ret == -EAGAIN) {
                // No events, introduce short wait to avoid busy looping
                std::this_thread::sleep_for(std::chrono::microseconds(RETRY_INTERVAL_US));
                elapsed_time += RETRY_INTERVAL_US;
            } else {
                // Handle errors
                log::error("RDMA tx cq read failed")
                          ("error", fi_strerror(-ret))("kind", kind2str(_kind));
                break; // Exit retry loop on error
            }

            if (ctx.cancelled()) {
                break;
            }
        }

        // Log if timeout occurred without receiving any events after buffer should be already
        // available
        if (elapsed_time >= TIMEOUT_US) {
            log::debug("RDMA tx cq read timed out after retries")("kind", kind2str(_kind));
        }
    }
        ep_ctx->stop_flag = true; // Set the stop flag
}

/**
 * @brief Handles sending data through RDMA by consuming a buffer, copying data, and transmitting it.
 * 
 * This function attempts to consume a pre-allocated buffer from the queue within a specified timeout,
 * copies the provided data into the buffer, and sends it through the RDMA endpoint. It ensures proper
 * error handling, retries for buffer availability, and buffer management in case of transmission failure.
 * 
 * @param ctx The context for managing the operation.
 * @param ptr Pointer to the data to be transmitted.
 * @param sz The size of the data to be transmitted.
 * @param sent Output parameter indicating the actual number of bytes sent.
 * @return Result::success on successful transmission, or an appropriate error result on failure.
 */
Result RdmaTx::on_receive(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent)
{
    void *reg_buf = nullptr;
    constexpr uint32_t TIMEOUT_US = 500000;     // 0.5-second timeout
    constexpr uint32_t RETRY_INTERVAL_US = 100; // Retry interval of 100 us
    uint32_t elapsed_time = 0;

    // Attempt to consume a buffer from the queue with a timeout
    while (elapsed_time < TIMEOUT_US && !ctx.cancelled()) {
        Result res = consume_from_queue(ctx, &reg_buf);
        if (res == Result::success) {
            if (reg_buf != nullptr) {
                break; // Successfully got a buffer
            } else {
                log::debug("RDMA tx buffer is null, retrying...")("kind", kind2str(_kind));
            }
        } else if (res != Result::error_no_buffer) {
            // Log non-retryable errors and exit
            log::error("RDMA tx failed to consume buffer from queue")("result", static_cast<int>(res))
                      ("kind", kind2str(_kind));
            sent = 0;
            return res;
        }

        // Wait before retrying
        std::this_thread::sleep_for(std::chrono::microseconds(RETRY_INTERVAL_US));
        elapsed_time += RETRY_INTERVAL_US;
    }

    // Check if we failed to get a buffer within the timeout
    if (reg_buf == nullptr) {
        log::error("RDMA tx failed to consume buffer within timeout")("timeout_ms", TIMEOUT_US)
                  ("kind", kind2str(_kind));
        sent = 0;
        return Result::error_timeout;
    }

    uint32_t tmp_sent = std::min(static_cast<uint32_t>(trx_sz), sz);
    if (tmp_sent != trx_sz) {
        log::debug("RDMA tx sent size differs from transfer size")("requested_size", tmp_sent)
                  ("trx_sz", trx_sz)("kind", kind2str(_kind));
    }

    std::memcpy(reg_buf, ptr, tmp_sent);

    // Transmit the buffer through RDMA
    int err = libfabric_ep_ops.ep_send_buf(ep_ctx, reg_buf, tmp_sent);
    notify_buf_available();
    sent = tmp_sent;

    if (err) {
        log::error("Failed to send buffer through RDMA tx")("error", fi_strerror(-err))
                  ("kind", kind2str(_kind));

        // Add the buffer back to the queue in case of failure
        Result res = add_to_queue(reg_buf);
        if (res != Result::success) {
            log::error("Failed to add buffer to RDMA tx queue")("error", result2str(res))
                      ("kind", kind2str(_kind));
        }

        sent = 0;
        return Result::error_general_failure;
    }

    return Result::success;
}

} // namespace mesh::connection