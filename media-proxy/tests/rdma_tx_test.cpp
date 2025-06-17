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
    void SetupRdmaConnectionsTx(size_t payload_size, int queue_size) {
        // Create new transmitter components
        conn_tx = new RdmaTx();
        emulated_tx = new EmulatedTransmitter(ctx);
        tx_dev_handle = nullptr;

        // Prepare connection params
        mcm_conn_param tx_request = {};
        tx_request.type = is_tx;
        tx_request.local_addr = {.ip = "192.168.2.20", .port = "9003"};  // adjust to your local
        tx_request.remote_addr = {.ip = "192.168.2.30", .port = "9002"}; // the remote Rx
        // tx_request.local_addr = {.ip = "192.168.2.30", .port = "9003"};  // adjust to your local
        // tx_request.remote_addr = {.ip = "192.168.2.20", .port = "9002"}; // the remote Rx
        tx_request.payload_args.rdma_args.transfer_size = payload_size;
        tx_request.payload_args.rdma_args.queue_size = queue_size;

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

/**
 * Test 2: Latency & Bandwidth over multiple (payload_size, queue_size) combos
 *
 * This mimics the original single-machine approach:
 * 1) For each payload_size & queue_size:
 *    - Cleanup (if existing) and re-Setup fresh Tx connections
 *    - Measure "latency" by timing a small number of transmissions
 *    - Measure "bandwidth" by sending a large total amount of data
 *    - Then Cleanup
 *
 * Note: Without a round-trip ACK from the Rx side, this is purely the local
 *       "post-send" time on the Tx side. If you want real end-to-end times,
 *       you must implement an ACK and wait for it here.
 */
TEST_F(RdmaRealEndpointsTxTest, LatencyAndBandwidthForVaryingPayloadSizesAndQueueSizes) {
    // Match the original single-machine values
    const size_t payload_sizes[] = {1 << 20, 8 << 20, 3840ULL * 2160ULL * 4ULL,
                                    64 << 20}; // 1MB, 8MB, ~32MB
    const int queue_sizes[] = {8, 16, 32, 64};
    const size_t total_data_size = 16ULL * 1024ULL * 1024ULL * 1024ULL; // 16 GB
    const size_t num_iterations = 200;
    const char filler = 'A';

    // Store results for: (PayloadMB, QueueSize, AvgLatencyMS, BandwidthGB/s)
    std::vector<std::tuple<double, int, double, double>> results;

    for (int qsz : queue_sizes) {
        for (size_t psz : payload_sizes) {
            printf("\nTesting payload size: %zu bytes, queue size: %d\n", psz, qsz);
            sleep(1);
            // 2) Set up a new Tx connection for this (psz, qsz)
            SetupRdmaConnectionsTx(psz, qsz);

            // Prepare data
            // Reserve space for a 4-byte frame number + payload
            const size_t header_size = sizeof(uint32_t);
            std::vector<char> test_data(header_size + psz - header_size, filler);

            // Frame counter
            uint32_t frame_number = 0;

            // --- Measure latency (local post-send time) ---
            double total_latency_ms = 0.0;
            for (size_t i = 0; i < num_iterations; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                // --- insert frame number in network byte-order at start of buffer ---
                uint32_t netfn = htonl(frame_number);
                std::memcpy(test_data.data(), &netfn, header_size);

                auto res = emulated_tx->transmit_plaintext(ctx, test_data.data(), test_data.size());
                EXPECT_EQ(res, connection::Result::success);
                frame_number++;

                // If you want real end-to-end times, you need an ACK from receiver here:
                // e.g. WaitForReceiverAck();

                auto end = std::chrono::high_resolution_clock::now();
                total_latency_ms += std::chrono::duration<double, std::milli>(end - start).count();
            }
            double avg_latency_ms = total_latency_ms / num_iterations;

            // --- Measure bandwidth ---
            size_t num_sends = total_data_size / psz;
            auto bdw_start = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < num_sends; ++i) {

                // keep numbering across the bandwidth test too,
                // and send in network-byte-order
                uint32_t netfn = htonl(frame_number);
                std::memcpy(test_data.data(), &netfn, header_size);

                auto res = emulated_tx->transmit_plaintext(ctx, test_data.data(), test_data.size());
                EXPECT_EQ(res, connection::Result::success);
                frame_number++;
                // WaitForReceiverAck(); // if doing real round-trip
            }

            auto bdw_end = std::chrono::high_resolution_clock::now();
            double elapsed_sec = std::chrono::duration<double>(bdw_end - bdw_start).count();

            double total_gb = (static_cast<double>(psz) * num_sends) / (1024.0 * 1024.0 * 1024.0);
            double throughput_gb_s = total_gb / elapsed_sec;

            // Save results
            results.emplace_back(static_cast<double>(psz) / (1024.0 * 1024.0), qsz, avg_latency_ms,
                                 throughput_gb_s);
            sleep(1); // Give some time before next test
            std::cout << "[TX] Completed payload size: " << psz / (1024 * 1024)
                      << " MB, queue size: " << qsz
                      << ", Avg Latency: " << avg_latency_ms
                      << " ms, Throughput: " << throughput_gb_s
                      << " GB/s\n";
            // 3) Clean up the connection after finishing this (psz, qsz)
            CleanupRdmaConnectionsTx();
        }
    }

    // Print results in a table
    std::cout
        << "\n+-------------------+-------------+--------------------+----------------------+\n";
    std::cout
        << "| Payload Size (MB) | Queue Size  |    Latency (ms)    | Transfer Rate (GB/s) |\n";
    std::cout
        << "+-------------------+-------------+--------------------+----------------------+\n";
    for (auto& entry : results) {
        std::cout << "| " << std::setw(17) << std::fixed << std::setprecision(2)
                  << std::get<0>(entry) << " | " << std::setw(11) << std::get<1>(entry) << " | "
                  << std::setw(18) << std::fixed << std::setprecision(3) << std::get<2>(entry)
                  << " | " << std::setw(20) << std::fixed << std::setprecision(3)
                  << std::get<3>(entry) << " |\n";
    }
    std::cout
        << "+-------------------+-------------+--------------------+----------------------+\n";
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
