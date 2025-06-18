#include "conn_rdma_rx.h"
#include <stdexcept>
#include <queue>
#include <immintrin.h>

namespace mesh::connection {

RdmaRx::RdmaRx() : Rdma() {
    _kind = Kind::receiver; // Set the Kind in the constructor
}

RdmaRx::~RdmaRx()
{
    // Any specific cleanup for Rx can go here
}

Result RdmaRx::configure(context::Context& ctx, const mcm_conn_param& request,
                         libfabric_ctx *& dev_handle)
{
    log::debug("RdmaRx configure")("local_ip", request.local_addr.ip)
                                  ("local_port", request.local_addr.port)
                                  ("remote_ip", request.remote_addr.ip)
                                  ("remote_port", request.remote_addr.port);

    return Rdma::configure(ctx, request, dev_handle);
}

Result RdmaRx::start_threads(context::Context& ctx) {

    process_buffers_thread_ctx = context::WithCancel(ctx);
    rdma_cq_thread_ctx = context::WithCancel(ctx);

    try {
        handle_process_buffers_thread = std::jthread(
            [this]() { this->process_buffers_thread(this->process_buffers_thread_ctx); });
    } catch (const std::system_error& e) {
        log::error("RDMA rx failed to start thread")("error", e.what())
                  ("kind", kind2str(_kind));
        return Result::error_thread_creation_failed;
    }

    try {
        handle_rdma_cq_thread =
            std::jthread([this]() { this->rdma_cq_thread(this->rdma_cq_thread_ctx); });
    } catch (const std::system_error& e) {
        log::error("RDMA rx failed to start thread")("error", e.what())
                  ("kind", kind2str(_kind));
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

        // Drain all currently queued buffers
        while (!ctx.cancelled()) {
            Result res = consume_from_queue(ctx, &buf);
            if (res == Result::success && buf) {
                // Round-robin receive postings across the two QPs
                uint32_t idx = next_rx_idx.fetch_add(1, std::memory_order_relaxed)
                             % ep_ctxs.size();
                ep_ctx_t* chosen = ep_ctxs[idx];
                if (!chosen) {
                    log::error("RDMA rx endpoint #%u is null, skipping buffer")
                        ("idx", idx)("kind", kind2str(_kind));
                    // Return buffer so it isn't lost
                    add_to_queue(buf);
                    break;
                }

                int err = libfabric_ep_ops.ep_recv_buf(chosen, buf, trx_sz + TRAILER, buf);
                if (err) {
                    log::error("Failed to post recv buffer to RDMA rx")
                        ("buffer_address", buf)
                        ("error", fi_strerror(-err))
                        ("kind", kind2str(_kind));
                    // On error, put the buffer back on the queue
                    res = add_to_queue(buf);
                    if (res != Result::success) {
                        log::error("Failed to re-queue buffer after recv error")
                            ("error", result2str(res))
                            ("kind", kind2str(_kind));
                        break;
                    }
                }
            } else {
                // No more buffers available right now
                break;
            }
        }

        // Wait until new buffers are added
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
void RdmaRx::rdma_cq_thread(context::Context& ctx) {
    constexpr int CQ_RETRY_DELAY_US = 100;
    struct fi_cq_entry cq_entries[CQ_BATCH_SIZE];

    while (!ctx.cancelled()) {
        bool did_work = false;


        // Poll each *unique* CQ only once
        struct fid_cq *last_cq = nullptr;
        for (auto* ep : ep_ctxs) {
            if (!ep) continue;

            struct fid_cq *cq = ep->cq_ctx.cq;
            if (cq == last_cq)           // duplicate of the one we just handled
                continue;
            last_cq = cq;

            int ret = fi_cq_read(cq, cq_entries, CQ_BATCH_SIZE);
            if (ret > 0) {
                did_work = true;

                for (int i = 0; i < ret; ++i) {
                    void* buf = cq_entries[i].op_context;
                    if (!buf) {
                        log::error("RDMA rx null buffer context, skipping...")
                            ("batch_index", i)
                            ("kind", kind2str(_kind));
                        continue;
                    }

                    // Read 64-bit trailer after payload
                    auto* trailer_ptr = reinterpret_cast<uint64_t*>(
                        reinterpret_cast<char*>(buf) + trx_sz);
                    uint64_t seq = *trailer_ptr;

                    if (reorder_head == UINT64_MAX) {
                        reorder_head = seq;
                    }
                    // Slot into ring buffer
                    size_t idx = seq & (REORDER_WINDOW - 1);
                    reorder_ring[idx] = buf;

                    // Flush any in-order entries
                    while (true) {
                        size_t head_idx = reorder_head & (REORDER_WINDOW - 1);
                        void* ready = reorder_ring[head_idx];
                        if (!ready) break;
                        reorder_ring[head_idx] = nullptr;

                        // Deliver payload (exclude trailer)
                        Result r = transmit(ctx, ready, trx_sz);
                        if (r != Result::success) {
                            log::error("RDMA rx failed to transmit buffer")
                                ("buffer_address", ready)
                                ("size", trx_sz)
                                ("kind", kind2str(_kind));
                        }

                        // Recycle buffer
                        r = add_to_queue(ready);
                        if (r == Result::success) {
                            notify_buf_available();
                        } else {
                            log::error("Failed to recycle buffer to queue")
                                ("buffer_address", ready)
                                ("kind", kind2str(_kind));
                        }
                        ++reorder_head;
                    }
                }
            }
            else if (ret == -FI_EAVAIL) {
                fi_cq_err_entry err_entry {};
                int err_ret = fi_cq_readerr(ep->cq_ctx.cq, &err_entry, 0);
                if (err_ret >= 0) {
                    int err = err_entry.err;

                    /* human-friendly diagnostics */
                    if (err == -FI_ECANCELED) {
                        log::warn("RDMA rx operation canceled")
                            ("error", fi_strerror(err))("kind", kind2str(_kind));
                    } else if (err == -FI_ECONNRESET || err == -FI_ENOTCONN) {
                        log::warn("RDMA connection reset/not connected; retrying")
                            ("error", fi_strerror(err))("kind", kind2str(_kind));
                        thread::Sleep(ctx, std::chrono::milliseconds(1000));
                    } else if (err == -FI_ECONNABORTED) {
                        log::warn("RDMA rx connection aborted")
                            ("error", fi_strerror(err))("kind", kind2str(_kind));
                    } else {
                        log::error("RDMA rx encountered CQ error")
                            ("error", fi_strerror(err))("kind", kind2str(_kind));
                    }

                    /* recycle the buffer that was canceled / errored */
                    if (err_entry.op_context) {
                        if (add_to_queue(err_entry.op_context) == Result::success)
                            notify_buf_available();
                        else
                            log::error("Failed to recycle buffer after CQ error")
                                ("buffer_address", err_entry.op_context)
                                ("kind", kind2str(_kind));
                    }

                    /* ---- if it was ECANCELED, repost done:  keep reorder_head,
                       just try to flush any packet that became in-order now ---- */
                    if (err == -FI_ECANCELED) {
                        std::size_t flushed = 0;
                        while (true) {
                            size_t head_idx = reorder_head & (REORDER_WINDOW - 1);
                            void*  ready    = reorder_ring[head_idx];
                            if (!ready) break;   // next frame not here yet
                            reorder_ring[head_idx] = nullptr;

                            Result r = transmit(ctx, ready, trx_sz);
                            if (r != Result::success)
                                log::error("RDMA rx failed to transmit buffer")
                                    ("buffer_address", ready)("size", trx_sz)
                                    ("kind", kind2str(_kind));

                            if (add_to_queue(ready) == Result::success)
                                notify_buf_available();
                            else
                                log::error("Failed to recycle buffer to queue")
                                    ("buffer_address", ready)("kind", kind2str(_kind));				
                            ++reorder_head;
                            ++flushed;
                        }
                        log::debug("RX ECANCELED: flushed %zu frame%s waiting in ring",
                                   flushed, flushed == 1 ? "" : "s")
                            ("kind", kind2str(_kind));
                    }

                    did_work = true;         // we handled something – no sleep
                } else {
                    log::error("RDMA rx failed to read CQ error entry")
                        ("error", fi_strerror(-err_ret))("kind", kind2str(_kind));
                }
            }
            else if (ret != -EAGAIN && ret != -FI_ENOTCONN) {
                // Fatal CQ read error
                log::error("RDMA rx cq read failed")
                    ("error", fi_strerror(-ret))
                    ("kind", kind2str(_kind));
                goto shutdown;
            }
            // else: -EAGAIN or -FI_ENOTCONN → retry
        }

        /* ---------- hybrid back-off when no CQ work was done ---------- */
        static constexpr int SPIN_LIMIT  =  50;   // ≈ 1–2 µs of busy-wait
        static constexpr int YIELD_LIMIT = 200;   // then ~200 sched_yield() calls
        static thread_local int idle_cycles = 0;  // per-thread counter

        if (!did_work) {
            if (idle_cycles < SPIN_LIMIT) {
                /* short spin: cheapest path at high packet rate */
                _mm_pause();             // pause instruction = 40–100 ns
            } else if (idle_cycles < SPIN_LIMIT + YIELD_LIMIT) {
                /* medium wait: let other threads run */
                std::this_thread::yield();
            } else {
                /* long wait: nothing for a while – real sleep */
                std::this_thread::sleep_for(
                    std::chrono::microseconds(CQ_RETRY_DELAY_US));
            }
            ++idle_cycles;               // back-off gets longer
        } else {
            idle_cycles = 0;             // reset after we did useful work
        }
    }

shutdown:
    // Signal all endpoints to stop
    for (auto* ep : ep_ctxs) {
        if (ep) ep->stop_flag = true;
    }
    log::info("RDMA RX CQ thread stopped.")("kind", kind2str(_kind));
}

} // namespace mesh::connection
