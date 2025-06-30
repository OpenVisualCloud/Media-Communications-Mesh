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

#include "metrics.h"               // FrameHdr + StatsMsg
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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
    bool     have_last_  = false;
    uint32_t last_frame_ = 0;
    std::atomic<uint64_t> missing_frames_{0};
    std::atomic<uint64_t> first_tx_ns{0};
    std::atomic<uint64_t> first_rx_ns{0};
    std::atomic<uint64_t> last_rx_ns{0};
    std::atomic<uint32_t> ttlb_seen{0};
    std::atomic<uint64_t> ttlb_ns_sum{0};
    std::atomic<uint64_t> first_ttlb_tx_ns{0};
    std::atomic<uint64_t> last_ttlb_rx_ns{0};
    // paced‐probe (phase A) stats
    std::atomic<uint64_t> ttlb_spaced_ns_sum{0};
    std::atomic<uint32_t> ttlb_spaced_seen  {0};
    std::vector<uint64_t> ttlb_spaced_samples_;

    // full‐speed (phase B) stats
    std::atomic<uint64_t> ttlb_full_ns_sum  {0};
    std::atomic<uint32_t> ttlb_full_seen    {0};
    std::vector<uint64_t> ttlb_full_samples_;

    std::vector<uint64_t> ttlb_samples_;
    void clearLatencySamples() {
    ttlb_spaced_samples_.clear();
    ttlb_full_samples_.clear();
    }
    const std::vector<uint64_t>& getTtlbSpacedSamples() const { return ttlb_spaced_samples_; }
    const std::vector<uint64_t>& getTtlbFullSamples()   const { return ttlb_full_samples_; }

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

    connection::Result on_receive(context::Context& /*ctx*/,
                                        void*         ptr,
                                        uint32_t      sz,
                                        uint32_t&     sent) override
    {
        static constexpr uint32_t TTLB_ITERS = 200;  // frames per phase

        if (sz < sizeof(FrameHdr)) {
            std::cerr << "[RX] Packet too small (" << sz << " B)\n";
            return connection::Result::error_bad_argument;
        }

        // parse header + timestamps
        auto*    hdr   = reinterpret_cast<FrameHdr*>(ptr);
        uint32_t frame = ntohl(hdr->frame);
        uint64_t tx_ns = be64toh(hdr->tx_ns);

        uint64_t rx_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                            std::chrono::high_resolution_clock::now())
                            .time_since_epoch()
                            .count();

        // —————— TTLB book-keeping ——————
        // Warmup phase: first 200 frames
        if (frame < TTLB_ITERS) {
            // ignore 
            }
            // Phase A: paced probes (frames 0…199)
            else if (frame < 2 * TTLB_ITERS) {
                uint64_t dt = rx_ns - tx_ns;
                ttlb_spaced_ns_sum.fetch_add(dt, std::memory_order_relaxed);
                ttlb_spaced_seen.fetch_add(1, std::memory_order_relaxed);
                ttlb_spaced_samples_.push_back(dt);
            }
            // Phase B: full-speed probes (frames 200…399)
            else if (frame < 3 * TTLB_ITERS) {
                uint64_t dt = rx_ns - tx_ns;
                ttlb_full_ns_sum.fetch_add(dt, std::memory_order_relaxed);
                ttlb_full_seen.fetch_add(1, std::memory_order_relaxed);
                ttlb_full_samples_.push_back(dt);
            }

            // —————— first / last arrival stamps (diagnostics) ——————
            {
                uint64_t exp = 0;
                first_tx_ns.compare_exchange_strong(exp, tx_ns, std::memory_order_relaxed);
                exp = 0;
                first_rx_ns.compare_exchange_strong(exp, rx_ns, std::memory_order_relaxed);
                last_rx_ns.store(rx_ns, std::memory_order_relaxed);
            }

            // —————— loss / ordering tracking (unchanged) ——————
            if (have_last_) {
                if (frame == last_frame_ + 1) {
                    // in‐order
                } else if (frame > last_frame_ + 1) {
                    uint32_t gap = frame - last_frame_ - 1;
                    missing_frames_.fetch_add(gap, std::memory_order_relaxed);
                    std::cerr << "[RX] Missing " << gap << " between " << last_frame_ << " and "
                              << frame << "\n";
                } else if (frame != last_frame_) {
                    std::cerr << "[RX] Out-of-order " << frame << " after " << last_frame_ << "\n";
                }
            } else {
                have_last_ = true;
            }
            last_frame_ = frame;

            // —————— count it and consume ——————
            received_count.fetch_add(1, std::memory_order_relaxed);
            sent = 0;
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

// ---------------------------------------------------------------------------
//  RdmaRealEndpointsRxTest
//  Matches the three-phase TX scheme:
//
//    • first  200 normal-size frames     → average **TTLB**
//    • remaining frames (to 16 GiB)     → throughput, no timing
//
//  The averages are accumulated inside PerfReceiver with
//     ttlb_seen / ttfb_ns_sum / ttlb_ns_sum
// ---------------------------------------------------------------------------
TEST_F(RdmaRealEndpointsRxTest, MultipleReception)
{
    /* --------------------------- test matrix ------------------------------ */
    const size_t payload_sizes[] = {
        568ULL * 320ULL * 4ULL,     // 320p
        1280ULL * 720ULL * 4ULL,    // 720p
        1920ULL * 1080ULL * 4ULL,   // 1080p
        3840ULL * 2160ULL * 4ULL    // 4K
    };
    const int    queue_sizes[]      = {1, 4, 16};
    char*        providers[]        = {"tcp", "verbs"};
    const int    endpoint_counts[]  = {1, 2, 4};

    const size_t TOTAL_STREAM_BYTES = 16ULL * 1024ULL * 1024ULL * 1024ULL; // 16 GiB

    /* --------------------------- constants -------------------------------- */
    static constexpr size_t TTLB_ITERS = 200;

    constexpr char TX_IP[]   = "192.168.2.20";
    constexpr int  METRICS_PORT = 9999;

    auto cpu_seconds = []() -> double {
        rusage ru{};  getrusage(RUSAGE_SELF, &ru);
        return ru.ru_utime.tv_sec  + ru.ru_utime.tv_usec/1e6 +
               ru.ru_stime.tv_sec  + ru.ru_stime.tv_usec/1e6;
    };

    /* ===================================================================== */
    for (auto prov : providers)
    for (int num_eps : endpoint_counts)
    for (int qsz : queue_sizes)
    for (size_t psz : payload_sizes)
    {
        if (prov == "tcp" && num_eps > 1) {
            std::cerr << "[RX] ⚠ TCP provider does not support multiple endpoints\n";
            continue;
        }

        /* ---- clean any prior connection --------------------------------- */
        if (conn_rx) {
            EXPECT_EQ(conn_rx->shutdown(ctx), connection::Result::success);
            delete conn_rx;  conn_rx = nullptr;
        }

        /* ---- create fresh RX endpoint ----------------------------------- */
        conn_rx = new RdmaRx();
        mcm_conn_param p{};  p.type = is_rx;
        p.local_addr = {.ip = "192.168.2.30", .port = "9002"};
        p.payload_args.rdma_args.transfer_size = psz;
        p.payload_args.rdma_args.queue_size    = qsz;
        p.payload_args.rdma_args.provider      = prov;
        p.payload_args.rdma_args.num_endpoints = num_eps;

        ASSERT_EQ(conn_rx->configure(ctx, p, rx_dev_handle),
                  connection::Result::success);
        ASSERT_EQ(conn_rx->establish(ctx), connection::Result::success);
        conn_rx->set_link(ctx, perf_rx);

        /* ---- reset per-run counters in PerfReceiver --------------------- */
        const size_t msgs_expected =
            TOTAL_STREAM_BYTES/psz + 3 * TTLB_ITERS;  // only TTLB probes

        perf_rx->received_count .store(0, std::memory_order_relaxed);
        perf_rx->missing_frames_.store(0, std::memory_order_relaxed);
        perf_rx->have_last_      = false;

        perf_rx->ttlb_full_ns_sum  .store(0, std::memory_order_relaxed);
        perf_rx->ttlb_full_seen    .store(0, std::memory_order_relaxed);
        perf_rx->ttlb_spaced_ns_sum.store(0, std::memory_order_relaxed);
        perf_rx->ttlb_spaced_seen  .store(0, std::memory_order_relaxed);
        perf_rx->clearLatencySamples();

        std::cout << "\n[RX] waiting for " << msgs_expected
                  << " msgs of " << (psz/1024/1024) << " MiB,"
                  << " q" << qsz
                  << " Prov " << prov
                  << " #EP " << num_eps << " …\n";

        /* ---- CPU-load anchors ------------------------------------------ */
        auto   wall_start = std::chrono::steady_clock::now();
        double cpu_start  = cpu_seconds();

        /* ---- wait until all expected frames have arrived --------------- */
        while (perf_rx->received_count.load(std::memory_order_relaxed)
               < msgs_expected)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        /* ---- compute TTLB average --------------------------------------- */
        double spaced_avg_ms =
            double(perf_rx->ttlb_spaced_ns_sum.load()) / perf_rx->ttlb_spaced_seen.load() / 1e6;
        double full_avg_ms =
            double(perf_rx->ttlb_full_ns_sum.load()) / perf_rx->ttlb_full_seen.load() / 1e6;

        auto   wall_end = std::chrono::steady_clock::now();
        double wall_sec = std::chrono::duration<double>(wall_end - wall_start).count();
        double cpu_pct  = 100.0*(cpu_seconds() - cpu_start)/wall_sec;

        /* percentile helper */
        auto computePercentileMs = [&](std::vector<uint64_t> v, double p) {
            if (v.empty()) return 0.0;
            size_t idx = size_t(p * (v.size() - 1));
            std::nth_element(v.begin(), v.begin() + idx, v.end());
            return double(v[idx]) / 1e6;
        };

        /* fetch TTLB samples */
        auto ttlb_samples = perf_rx->getTtlbFullSamples();

        /* print TTLB percentiles */
        std::cout << "[RX] TTLB avg=" << full_avg_ms << " ms  "
                  << "P25=" << computePercentileMs(ttlb_samples, 0.25) << " ms  "
                  << "P50=" << computePercentileMs(ttlb_samples, 0.50) << " ms  "
                  << "P90=" << computePercentileMs(ttlb_samples, 0.90) << " ms  "
                  << "P99=" << computePercentileMs(ttlb_samples, 0.99) << " ms\n";

        std::cout << "[RX] done " << (psz/1024/1024)
                  << " MiB,q" << qsz
                  << "  missing=" << perf_rx->missing_frames_.load()
                  << "  TTLB=" << full_avg_ms
                  << " ms CPU=" << cpu_pct << "%\n";

        /* ---- send StatsMsg to the transmitter -------------------------- */
        StatsMsg sm{
            .payload_mb     = static_cast<uint32_t>(psz/(1024*1024)),
            .queue          = static_cast<uint32_t>(qsz),
            .ttlb_spaced_ms = spaced_avg_ms,
            .ttlb_full_ms   = full_avg_ms,
            .cpu_tx_pct     = 0.0,
            .cpu_rx_pct     = cpu_pct
        };

        int s = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in tx{};  tx.sin_family = AF_INET;
        tx.sin_port = htons(METRICS_PORT);
        inet_pton(AF_INET, TX_IP, &tx.sin_addr);
        sendto(s, &sm, sizeof(sm), 0,
               reinterpret_cast<sockaddr*>(&tx), sizeof(tx));
        close(s);
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
