#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>

// Include your RDMA Rx and logging interfaces
#include "mesh/conn_rdma_rx.h"
#include "mesh/concurrency.h"
#include "logger.h"
#include <arpa/inet.h>

using namespace mesh::log;

namespace mesh {
namespace connection {

/** Logging level (optional) **/
Level log_level = Level::fatal;

/** --------------------------------------------------------------------
 *  PerfReceiver:
 *     - Extends Connection to handle incoming data with minimal overhead
 *     - Increments a counter per receive, does no string copy or print
 * ------------------------------------------------------------------ **/
class PerfReceiver : public connection::Connection {
public:
    std::atomic<uint64_t> received_count{0};
    // Track last frame and total missing
    bool     have_last_     = false;
    uint32_t last_frame_    = 0;
    std::atomic<uint64_t> missing_frames_{0};

    PerfReceiver(context::Context& ctx)
    {
        _kind = connection::Kind::receiver;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context& ctx) override
    {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context& ctx) override
    {
        return connection::Result::success;
    }

    connection::Result on_receive(context::Context& ctx, void* ptr, uint32_t sz,
                                  uint32_t& sent) override
    {
        // 1) pull off the 4-byte header (network order → host order)
        uint32_t netframe;
        std::memcpy(&netframe, ptr, sizeof(netframe));
        uint32_t frame = ntohl(netframe);

        if (have_last_) {
            if (frame == last_frame_) {
                // duplicate—ignore
            }
            else if (frame == last_frame_ + 1) {
                // perfectly consecutive—nothing to do
            }
            else if (frame > last_frame_ + 1) {
                uint32_t gap = frame - last_frame_ - 1;
                missing_frames_.fetch_add(gap, std::memory_order_relaxed);
                std::cerr << "[RX] Missing " << gap
                          << " frame(s) between " << last_frame_
                          << " and " << frame << "\n";
            }
            else {
                // optional: handle wrap-around or out-of-order
                std::cerr << "[RX] Unexpected frame order: "
                          << frame << " after " << last_frame_ << "\n";
            }
        }
        else {
            have_last_ = true;  // first packet, just seed last_frame_
        }
        last_frame_ = frame;

        // 3) Count the valid receipt
        received_count.fetch_add(1, std::memory_order_relaxed);
        return connection::Result::success;
    }

    connection::Result configure(context::Context& ctx)
    {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }
};

/** --------------------------------------------------------------------
 *  RdmaRealEndpointsRxTest:
 *     - GTest fixture for the RX side
 * ------------------------------------------------------------------ **/
class RdmaRealEndpointsRxTest : public ::testing::Test {
protected:
    context::Context ctx;
    RdmaRx* conn_rx;
    PerfReceiver* perf_rx;
    libfabric_ctx* rx_dev_handle;
    std::atomic<bool> keep_running;

    void SetUp() override
    {
        ctx = context::WithCancel(context::Background());

        // Create RDMA receiver connection
        conn_rx     = new RdmaRx();
        perf_rx     = new PerfReceiver(ctx);
        rx_dev_handle = nullptr;
        keep_running = true;

        // Adjust these for your environment:
        // A larger transfer_size ensures the posted buffer is big enough
        // for the largest messages you expect from the transmitter.
        // A bigger queue_size can boost throughput by posting multiple receives.
        mcm_conn_param rx_request = {};
        rx_request.type = is_rx;
        rx_request.local_addr = {.ip = "192.168.2.30", .port = "9002"};
        // rx_request.local_addr = {.ip = "192.168.2.20", .port = "9002"};
        rx_request.payload_args.rdma_args.transfer_size = 3840ULL * 2160ULL * 4ULL;
        rx_request.payload_args.rdma_args.queue_size    = 64;

        // Configure & establish the RDMA Rx
        ASSERT_EQ(conn_rx->configure(ctx, rx_request, rx_dev_handle),
                  connection::Result::success);
        ASSERT_EQ(conn_rx->establish(ctx), connection::Result::success);

        // Configure & establish our PerfReceiver
        ASSERT_EQ(perf_rx->configure(ctx), connection::Result::success);
        ASSERT_EQ(perf_rx->establish(ctx), connection::Result::success);

        // Link the RDMA Rx to our PerfReceiver
        conn_rx->set_link(ctx, perf_rx);
    }

    void TearDown() override
    {
        keep_running = false;
        ASSERT_EQ(conn_rx->shutdown(ctx), connection::Result::success);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        delete conn_rx;
        delete perf_rx;
    }
};

/**
 * Test: MultipleReception
 * - Receive a certain number of messages (or run indefinitely).
 * - We simply loop until we see enough messages arrive. 
 * - Because PerfReceiver doesn't copy or log data, overhead is minimized.
 */
TEST_F(RdmaRealEndpointsRxTest, MultipleReception)
{
    /* ---------- payload / queue parameters MUST match the Tx test ---------- */
    const size_t total_data_size = 16ULL * 1024ULL * 1024ULL * 1024ULL;  // 16 GiB
    const size_t payload_sizes[] = {1 << 20, 8 << 20, 3840ULL * 2160ULL * 4ULL, 64 << 20};
    const int    queue_sizes[]   = {8, 16, 32, 64};
    const size_t msgs_per_size   = 2200;          // how many packets to wait for

    for (int qsz : queue_sizes) {
        for (size_t psz : payload_sizes) {

            /* --- tear down any previous connection ---- */
            if (conn_rx) {
                ASSERT_EQ(conn_rx->shutdown(ctx), connection::Result::success);
                delete conn_rx;
                conn_rx = nullptr;
            }

            /* --- create a fresh Rx endpoint that uses the current payload size --- */
            conn_rx = new RdmaRx();
            mcm_conn_param rx_request = {};
            rx_request.type = is_rx;
            rx_request.local_addr = {.ip = "192.168.2.30", .port = "9002"};
            // rx_request.local_addr = {.ip = "192.168.2.20", .port = "9002"};
            rx_request.payload_args.rdma_args.transfer_size = psz;   // <-- match Tx
            rx_request.payload_args.rdma_args.queue_size    = qsz;

            ASSERT_EQ(conn_rx->configure(ctx, rx_request, rx_dev_handle),
                      connection::Result::success);
            ASSERT_EQ(conn_rx->establish(ctx), connection::Result::success);

            /* re-link to the same PerfReceiver */
            conn_rx->set_link(ctx, perf_rx);

            /* reset counters */
            const size_t msgs_expected = total_data_size / psz;
            perf_rx->received_count.store(0, std::memory_order_relaxed);
            perf_rx->missing_frames_.store(0, std::memory_order_relaxed);
            perf_rx->have_last_  = false;

            std::cout << "\n[RX] waiting for ≈" << msgs_expected + 200
                      << " messages of " << (psz / (1024*1024)) << " MiB, queue "
                      << qsz << " …" << std::endl;

            /* ------- busy-wait until the desired number of packets arrives ---- */
            while (perf_rx->received_count.load(std::memory_order_relaxed) <
                   msgs_expected + 200)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            std::cout << "[RX] done for payload "
                      << (psz / (1024*1024)) << " MiB, queue "
                      << qsz << ", missing = "
                      << perf_rx->missing_frames_.load(std::memory_order_relaxed)
                      << std::endl;
        }
    }
}

} // namespace connection
} // namespace mesh

/**
 * main() for the RX side test executable.
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
