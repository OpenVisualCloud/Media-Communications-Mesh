#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <cstring>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>

// Include your RDMA Tx and logging interfaces
#include "mesh/conn_rdma_tx.h"
#include "mesh/concurrency.h"
#include "logger.h"
#include <arpa/inet.h>
#include "metrics.h"
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
 *  EmulatedTransmitter:
 *     - Minimal transmitter that sends data.
 * ------------------------------------------------------------------ **/
class EmulatedTransmitter : public connection::Connection {
public:
    EmulatedTransmitter(context::Context& ctx) {
        _kind = connection::Kind::transmitter;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context& ctx) override {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context& ctx) override {
        return connection::Result::success;
    }

    connection::Result configure(context::Context& ctx) {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }

    connection::Result transmit_plaintext(context::Context& ctx, const void *ptr, size_t sz) {
        // RDMA transmit expects a void*, so cast away const
        return transmit(ctx, const_cast<void *>(ptr), sz);
    }
};

/** --------------------------------------------------------------------
 *  RdmaRealEndpointsTxTest:
 *     - GTest fixture for the TX side
 * ------------------------------------------------------------------ **/
class RdmaRealEndpointsTxTest : public ::testing::Test {
protected:
    context::Context ctx;
    RdmaTx *conn_tx;
    EmulatedTransmitter *emulated_tx;
    libfabric_ctx *tx_dev_handle;
    std::atomic<bool> keep_running;

    void SetUp() override {
        // We won't establish a connection here, because we want to mimic
        // the single-machine approach of re-creating connections for each
        // (payload_size, queue_size) pair. We'll do that in the test itself.
        ctx = context::WithCancel(context::Background());
        conn_tx = nullptr;
        emulated_tx = nullptr;
        tx_dev_handle = nullptr;
        keep_running = true;
    }

    void TearDown() override {
        // If a connection is active at the end, clean it up here
        if (conn_tx || emulated_tx) {
            CleanupRdmaConnectionsTx();
        }
    }

public:
    /**
     * SetupRdmaConnectionsTx:
     * Similar to single-machine SetupRdmaConnections, but only
     * creates and configures the TX side (since RX is on another machine).
     */
    void SetupRdmaConnectionsTx(size_t payload_size,
                                int queue_size,
                                char* provider_name,
                                int    num_endpoints) {
        // Create new transmitter components
        conn_tx = new RdmaTx();
        emulated_tx = new EmulatedTransmitter(ctx);
        tx_dev_handle = nullptr;

        // Prepare connection params
        mcm_conn_param tx_request = {};
        tx_request.type = is_tx;
        tx_request.local_addr = {.ip = "192.168.2.20", .port = "9003"};  // adjust to your local
        tx_request.remote_addr = {.ip = "192.168.2.30", .port = "9002"}; // the remote Rx
        tx_request.payload_args.rdma_args.transfer_size = payload_size;
        tx_request.payload_args.rdma_args.queue_size = queue_size;
        tx_request.payload_args.rdma_args.provider = provider_name;
        tx_request.payload_args.rdma_args.num_endpoints = num_endpoints;        

        // Configure & establish
        ASSERT_EQ(conn_tx->configure(ctx, tx_request, tx_dev_handle), connection::Result::success);
        ASSERT_EQ(conn_tx->establish(ctx), connection::Result::success);

        // Configure & establish the emulated Tx
        ASSERT_EQ(emulated_tx->configure(ctx), connection::Result::success);
        ASSERT_EQ(emulated_tx->establish(ctx), connection::Result::success);

        // Link them
        emulated_tx->set_link(ctx, conn_tx);

        keep_running = true;
    }

    /**
     * CleanupRdmaConnectionsTx:
     * Mimic the single-machine CleanupRdmaConnections approach
     */
    void CleanupRdmaConnectionsTx() {
        keep_running = false;
        // In single-machine code, there's an Rx as well; here we only handle Tx side
        if (conn_tx) {
            ASSERT_EQ(conn_tx->shutdown(ctx), connection::Result::success);
        }
        // Give some time for the shutdown
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));

        delete conn_tx;
        delete emulated_tx;

        conn_tx = nullptr;
        emulated_tx = nullptr;
    }
};

// /**
//  * Test 1: Basic Transmission Example
//  * - Sends a small amount of data 5 times.
//  *   (Adapted from simpler usage.)
//  */
// TEST_F(RdmaRealEndpointsTxTest, ConcurrentTransmission)
// {
//     // Setup connections with some default size & queue
//     SetupRdmaConnectionsTx(/*payload_size*/ 1024, /*queue_size*/ 8);

//     std::thread tx_thread([&]() {
//         const char* test_data = "Hello RDMA World!";
//         size_t data_size      = strlen(test_data) + 1;

//         for (int i = 0; i < 5 && keep_running; ++i) {
//             auto res = emulated_tx->transmit_plaintext(ctx, test_data, data_size);
//             EXPECT_EQ(res, connection::Result::success);
//             std::cout << "[TX] Sent iteration #" << (i + 1)
//                       << " data: " << test_data << std::endl;
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
//         keep_running = false;
//     });

//     tx_thread.join();

//     // Cleanup when done
//     CleanupRdmaConnectionsTx();
// }

// ---------------------------------------------------------------------------
//  RdmaRealEndpointsTxTest
//  End-to-end matrix with
//      • 200 × 16-byte frames → average **TTFB**
//      • 200 × normal-size frames → average **TTLB**
//      • 16 GiB raw stream      → **Throughput + CPU-load**
//  The RX side calculates the two latencies and ships them back in a
//  single StatsMsg; the TX side prints the full table.
// ---------------------------------------------------------------------------
TEST_F(RdmaRealEndpointsTxTest,
       LatencyAndBandwidthForVaryingPayloadSizesAndQueueSizes)
{
    /* ---------- test matrix ------------------------------------------------- */
    const size_t payload_sizes[] = {568ULL * 320ULL * 4ULL,     // 320p
                                    1280ULL * 720ULL * 4ULL,    // 720p
                                    1920ULL * 1080ULL * 4ULL,   // 1080p
                                    3840ULL * 2160ULL * 4ULL};  // 4k
    const int    queue_sizes[]  = {1, 4, 16};
    char* providers[]      = { "tcp", "verbs" };
    const int   endpoint_counts[] = { 1, 2, 4 };

    const size_t TOTAL_STREAM_BYTES = 16ULL * 1024ULL * 1024ULL * 1024ULL; // 16 GiB

    /* ---------- latency-probe parameters ------------------------------------ */
    static constexpr size_t TTLB_ITERS = 200;   // normal frames

    const char  filler = 'A';

    /* ---------- helpers ----------------------------------------------------- */
    constexpr int METRICS_PORT = 9999;

    auto cpu_seconds = []() -> double {
        rusage ru{}; getrusage(RUSAGE_SELF, &ru);
        return ru.ru_utime.tv_sec  + ru.ru_utime.tv_usec/1e6 +
               ru.ru_stime.tv_sec  + ru.ru_stime.tv_usec/1e6;
    };
    auto now_ns = []() -> uint64_t {
        return std::chrono::time_point_cast<std::chrono::nanoseconds>(
                   std::chrono::high_resolution_clock::now())
               .time_since_epoch().count();
    };

    /* ---------- UDP socket for StatsMsg ------------------------------------- */
    int stat_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in si{};  si.sin_family = AF_INET;
    si.sin_addr.s_addr = INADDR_ANY;
    si.sin_port        = htons(METRICS_PORT);
    bind(stat_sock, reinterpret_cast<sockaddr*>(&si), sizeof(si));
    timeval tv{.tv_sec = 5, .tv_usec = 0};
    setsockopt(stat_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // { provider, num_endpoints, payload_mb, queue_size,
    //   ttlb_spaced_ms, ttlb_full_ms, throughput_gbps,
    //   cpu_tx_pct, cpu_rx_pct }
    std::vector<
        std::tuple<
            std::string,  // provider
            int,          // #endpoints
            double,       // payload size (MB)
            int,          // queue size
            double,       // TTLB @60 fps (ms)
            double,       // TTLB full-speed (ms)
            double,       // throughput (GB/s)
            double,       // CPU-TX (%)
            double        // CPU-RX (%)
        >
    > results;

    /* ======================================================================= */
    for (auto prov : providers)
    for (int num_eps : endpoint_counts)    
    for (int qsz : queue_sizes)
    for (size_t psz : payload_sizes)
    {
        if (prov == "tcp" && num_eps > 1) {
            std::cerr << "[TX] ⚠ TCP provider does not support multiple endpoints\n";
            continue; // skip TCP with multiple endpoints
        }

        std::cout << "\n[TX] payload " << (psz / 1024 / 1024) << " MB, queue " << qsz << " Prov "
                  << prov << " #EP " << num_eps << " …\n";
        sleep(1);

        /* ---- fresh RDMA connection --------------------------------------- */
        SetupRdmaConnectionsTx(psz, qsz, prov, num_eps);

        /* ---- pre-allocate buffers ---------------------------------- */

        std::vector<char> buf_big(psz, filler);                 // phase A
        auto* hdr_big  = reinterpret_cast<FrameHdr*>(buf_big.data());

        std::vector<char> buf_raw(psz, filler);                 // phase B
        auto* hdr_raw  = reinterpret_cast<FrameHdr*>(buf_raw.data());
        hdr_raw->tx_ns = 0;                                     // no timestamp

        uint32_t frame = 0;

        /* ---------------------- Warmup Phase ------------------ */
        for (size_t i = 0; i < TTLB_ITERS; ++i) {
            hdr_big->frame = htonl(frame++);
            hdr_big->tx_ns = htobe64(now_ns());
            EXPECT_EQ(emulated_tx->transmit_plaintext(ctx,
                          buf_big.data(), buf_big.size()),
                      connection::Result::success);
        }

        sleep(0.2);        

        /* ---------------------- (A)  normal-probe → TTLB ------------------ */
        for (size_t i = 0; i < TTLB_ITERS; ++i) {
            hdr_big->frame = htonl(frame++);
            hdr_big->tx_ns = htobe64(now_ns());
            EXPECT_EQ(emulated_tx->transmit_plaintext(ctx,
                          buf_big.data(), buf_big.size()),
                      connection::Result::success);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        sleep(0.2);

        /* ---------------------- (B)  normal-probe → TTLB ------------------ */
        for (size_t i = 0; i < TTLB_ITERS; ++i) {
            hdr_big->frame = htonl(frame++);
            hdr_big->tx_ns = htobe64(now_ns());
            EXPECT_EQ(emulated_tx->transmit_plaintext(ctx,
                          buf_big.data(), buf_big.size()),
                      connection::Result::success);
        }

        sleep(0.2);

        /* ---------------------- (C)  raw throughput ----------------------- */
        size_t sends_needed = TOTAL_STREAM_BYTES / psz;
        auto   thr_start    = std::chrono::steady_clock::now();
        double cpu_start    = cpu_seconds();

        for (size_t i = 0; i < sends_needed; ++i) {
            hdr_raw->frame = htonl(frame++);    // still unique, no ts
            EXPECT_EQ(emulated_tx->transmit_plaintext(ctx,
                          buf_raw.data(), buf_raw.size()),
                      connection::Result::success);
        }

        sleep(0.2);

        auto   thr_end   = std::chrono::steady_clock::now();
        double cpu_end   = cpu_seconds();
        double thr_sec   = std::chrono::duration<double>(thr_end - thr_start).count();
        double cpu_tx_pct = 100.0 * (cpu_end - cpu_start) / thr_sec;

        double total_gib = static_cast<double>(psz) * sends_needed /
                           (1024.0*1024.0*1024.0);
        double gbps = total_gib / thr_sec;

        /* ---- wait for StatsMsg from RX ----------------------------------- */
        StatsMsg sm{};
        sockaddr_in src{}; socklen_t slen = sizeof(src);
        ssize_t n = recvfrom(stat_sock, &sm, sizeof(sm), 0,
                             reinterpret_cast<sockaddr*>(&src), &slen);

        if (n == sizeof(sm)) {
            results.emplace_back(
                                 std::string(prov),                    // provider
                                 num_eps,                              // #endpoints
                                 static_cast<double>(psz)/(1024*1024), // payload MB
                                 qsz,                                  // queue
                                 sm.ttlb_spaced_ms, sm.ttlb_full_ms,   // TTLB
                                 gbps,                                 // throughput
                                 cpu_tx_pct, sm.cpu_rx_pct);           // CPU

            std::cout << "[TX]" << " ms  ttlb @60fps=" << sm.ttlb_spaced_ms
                      << " ms  ttlb @max=" << sm.ttlb_full_ms
                      << " ms  thr=" << gbps << " GB/s  CPU-TX=" << cpu_tx_pct
                      << "%  CPU-RX=" << sm.cpu_rx_pct << "%\n";
        } else {
            std::cerr << "[TX] ⚠ no StatsMsg for "
                      << (psz/1024/1024) << " MB,q" << qsz << '\n';
        }

        CleanupRdmaConnectionsTx();
        sleep(1);
    }
    /* ======================================================================= */

    /* ---------- pretty-print table ----------------------------------------- */
    std::cout << "\n+----------+-----------+-------------------+-------------+-----------+--------------+--------------------+------------+------------+\n"
            <<   "| Provider | #Endpoints| Payload Size (MB) | Queue Size  | TTLB (ms) |   TTLB (ms)  | Maximum Throughput | CPU-TX (%) | CPU-RX (%) |\n"
            <<   "|          |           |                   |             |  @60 fps  |   @max thr.  |       (GB/s)       |     (100% is 1 core)    |\n"
            <<   "+----------+-----------+-------------------+-------------+-----------+--------------+--------------------+------------+------------+\n";

    for (const auto& r : results) {
        std::cout << "| " << std::setw(8)  << std::get<0>(r)                             // provider
                << " | " << std::setw(9)  << std::get<1>(r)                             // #endpoints
                << " | " << std::setw(17) << std::fixed << std::setprecision(2)
                                    << std::get<2>(r)                             // payload MB
                << " | " << std::setw(11) << std::get<3>(r)                             // queue
                << " | " << std::setw(9)  << std::fixed << std::setprecision(3)
                                    << std::get<4>(r)                             // TTLB spaced
                << " | " << std::setw(12) << std::fixed << std::setprecision(3)
                                    << std::get<5>(r)                             // TTLB full-speed
                << " | " << std::setw(18) << std::fixed << std::setprecision(3)
                                    << std::get<6>(r)                             // throughput
                << " | " << std::setw(10) << std::fixed << std::setprecision(1)
                                    << std::get<7>(r)                             // CPU-TX
                << " | " << std::setw(10) << std::fixed << std::setprecision(1)
                                    << std::get<8>(r)                             // CPU-RX
                << " |\n";
    }

    std::cout <<   "+----------+-----------+-------------------+-------------+-----------+--------------+--------------------+------------+------------+\n";


}

} // namespace connection
} // namespace mesh

/**
 * main() for the TX side test executable.
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
