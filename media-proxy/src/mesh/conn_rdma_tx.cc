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
    constexpr uint32_t RETRY_INTERVAL_US = 100;     // 100 µs back-off
    constexpr uint32_t TIMEOUT_US        = 1'000'000; // 1 s max spin

    // Buffer for batched completions
    struct fi_cq_entry cq_entries[CQ_BATCH_SIZE];

    while (!ctx.cancelled()) {
        // Wait until at least one send has occurred
        wait_buf_available();

        uint32_t elapsed = 0;
        bool     done    = false;

        while (!ctx.cancelled() && elapsed < TIMEOUT_US) {
        /* We use a single CQ for all QPs – skip duplicates */
        struct fid_cq *last_cq = nullptr;
        for (auto *ep : ep_ctxs) {
            if (!ep) continue;

            struct fid_cq *cq = ep->cq_ctx.cq;
            if (cq == last_cq)           // already handled in this iteration
                continue;
            last_cq = cq;

            int ret = fi_cq_read(cq, cq_entries, CQ_BATCH_SIZE);
                if (ret > 0) {
                    // We got completions on this QP
                    for (int i = 0; i < ret; ++i) {
                        void *buf = cq_entries[i].op_context;
                        if (!buf) {
                            log::error("RDMA tx null buffer context, skipping...")
                                ("kind", kind2str(_kind));
                            continue;
                        }
                        if (add_to_queue(buf) != Result::success) {
                            log::error("RDMA tx failed to add buffer back to queue")
                                ("buffer_address", buf)
                                ("kind", kind2str(_kind));
                        }
                    }
                    done = true;
                }
                else if (ret == -FI_EAVAIL) {
                    // asynchronous error completions ― recycle buffers too
                    fi_cq_err_entry err {};
                    if (fi_cq_readerr(ep->cq_ctx.cq, &err, 0) >= 0) {
                        log::error("RDMA tx CQ error")("error", fi_strerror(err.err))
                            ("kind", kind2str(_kind));
                        if (err.op_context) {
                            add_to_queue(err.op_context); // reclaim the failed buffer
                        }
                    } else {
                        log::error("RDMA tx failed to read CQ error entry")
                            ("kind", kind2str(_kind));
                    }
                    done = true;
                }
                else if (ret != -EAGAIN) {
                    // fatal CQ read error
                    log::error("RDMA tx cq read failed")
                        ("error", fi_strerror(-ret))
                        ("kind", kind2str(_kind));
                    done = true;
                }

                if (done) break;     // processed something on this iteration
            }

            if (done) break;         // go back to outer loop

            // No events yet on either CQ, back off a bit
            std::this_thread::sleep_for(std::chrono::microseconds(RETRY_INTERVAL_US));
            elapsed += RETRY_INTERVAL_US;
        }

        if (elapsed >= TIMEOUT_US) {
            log::debug("RDMA tx cq read timed out after retries")
                ("kind", kind2str(_kind));
        }
    }

    // Signal shutdown on all endpoints
    for (auto *ep : ep_ctxs) {
        if (ep) ep->stop_flag = true;
    }
    log::info("RDMA TX CQ thread stopped.")("kind", kind2str(_kind));
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
    constexpr uint32_t TIMEOUT_US         = 1000000; // 1-second timeout
    constexpr uint32_t RETRY_INTERVAL_US = 100;     // 100 µs
    uint32_t elapsed = 0;

    // 1) Acquire a buffer from our pool, with timeout
    while (elapsed < TIMEOUT_US && !ctx.cancelled()) {
        Result r = consume_from_queue(ctx, &reg_buf);
        if (r == Result::success) {
            if (reg_buf) break;
            log::debug("RDMA tx buffer is null, retrying...")("kind", kind2str(_kind));
        }
        else if (r != Result::error_no_buffer) {
            log::error("RDMA tx failed to consume buffer from queue")
                ("result", static_cast<int>(r))("kind", kind2str(_kind));
            sent = 0;
            return r;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(RETRY_INTERVAL_US));
        elapsed += RETRY_INTERVAL_US;
    }

    if (!reg_buf) {
        log::error("RDMA tx failed to consume buffer within timeout")
            ("timeout_us", TIMEOUT_US)("kind", kind2str(_kind));
        sent = 0;
        return Result::error_timeout;
    }

    // 2) Copy payload and pad to trx_sz
    char* data_ptr = reinterpret_cast<char*>(reg_buf);
    uint32_t to_send = std::min<uint32_t>(trx_sz, sz);
    std::memcpy(data_ptr, ptr, to_send);
    if (to_send < trx_sz)                       // pad any unused space
        std::memset(data_ptr + to_send, 0, trx_sz - to_send);

    // ---- write trailer at fixed offset ----
    uint64_t seq = global_seq.fetch_add(1, std::memory_order_relaxed);
    *reinterpret_cast<uint64_t*>(data_ptr + trx_sz) = seq;

    // Always send full payload-slot + trailer
    uint32_t total_len = trx_sz + static_cast<uint32_t>(TRAILER);

    uint32_t idx = next_tx_idx.fetch_add(1, std::memory_order_relaxed)
                   % static_cast<uint32_t>(ep_ctxs.size());
    ep_ctx_t* chosen = ep_ctxs[idx];
    if (!chosen) {
        log::error("RDMA tx endpoint #%u is null, cannot send")("idx", idx);
        sent = 0;
        add_to_queue(reg_buf);
        return Result::error_general_failure;
    }

    int rc = libfabric_ep_ops.ep_send_buf(chosen, reg_buf, total_len);
    // Signal that there’s now room for more sends
    notify_buf_available();
    sent = to_send;


    if (rc) {
        log::error("Failed to send buffer through RDMA tx")
            ("error", fi_strerror(-rc))("kind", kind2str(_kind));
        // Return buffer to pool
        Result qr = add_to_queue(reg_buf);
        if (qr != Result::success) {
            log::error("Failed to return buffer to queue after send error")
                ("error", result2str(qr))("kind", kind2str(_kind));
        }
        sent = 0;
        return Result::error_general_failure;
    }

    return Result::success;
}


} // namespace mesh::connection