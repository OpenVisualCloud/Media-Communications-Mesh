#include <gtest/gtest.h>
#include <thread>
#include "mesh/conn_rdma_rx.h"
#include "mesh/conn_rdma_tx.h"
#include "logger.h"
#include <cstring>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "mesh/concurrency.h"
#include <iomanip>

using namespace mesh::log;

namespace mesh {
namespace connection {

Level log_level = Level::fatal;

class EmulatedReceiver : public connection::Connection {
  public:
    EmulatedReceiver(context::Context& ctx)
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

    connection::Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                                  uint32_t& sent) override
    {
        ////log::info("Data received")("size", sz)("data", static_cast<char *>(ptr));
        return connection::Result::success;
    }

    connection::Result configure(context::Context& ctx)
    {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }
};

// Custom Receiver Class for Test
class TestReceiver : public EmulatedReceiver {
  public:
    std::string received_data;
    std::atomic<bool> data_received;
    std::mutex mtx;
    std::condition_variable cv;

    TestReceiver(context::Context& ctx) : EmulatedReceiver(ctx), data_received(false) {}

    connection::Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                                  uint32_t& sent) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        received_data = std::string(static_cast<char *>(ptr), sz);
        data_received = true;
        cv.notify_all();
        //////log::info("Data received via TestReceiver")("size", sz)("data", received_data);
        return connection::Result::success;
    }

    void wait_for_data()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return data_received.load(); });
    }
};

class EmulatedTransmitter : public connection::Connection {
  public:
    EmulatedTransmitter(context::Context& ctx)
    {
        _kind = connection::Kind::transmitter;
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

    connection::Result configure(context::Context& ctx)
    {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }

    connection::Result transmit_plaintext(context::Context& ctx, const void *ptr, size_t sz)
    {
        // Cast const void* to void* safely for transmit
        return transmit(ctx, const_cast<void *>(ptr), sz);
    }
};



class RdmaRealEndpointsTest : public ::testing::Test {
  protected:
    context::Context ctx;
    RdmaRx *conn_rx;
    RdmaTx *conn_tx;
    TestReceiver *test_rx;
    EmulatedTransmitter *emulated_tx;
    libfabric_ctx *rx_dev_handle;
    libfabric_ctx *tx_dev_handle;

    std::atomic<bool> keep_running;

    void SetupRdmaConnections(size_t payload_size, int queue_size) {
    
    ctx = context::WithCancel(context::Background());
    // Initialize connections
    conn_rx = new RdmaRx();
    conn_tx = new RdmaTx();
    test_rx = new TestReceiver(ctx);
    emulated_tx = new EmulatedTransmitter(ctx);

    libfabric_ctx *rx_dev_handle = NULL;
    libfabric_ctx *tx_dev_handle = NULL;

    // Set up RDMA Rx
    mcm_conn_param rx_request = {};
    rx_request.type = is_rx;
    rx_request.local_addr = {.ip = "192.168.1.22", .port = "8002"};
    rx_request.payload_args.rdma_args.transfer_size = payload_size;
    rx_request.payload_args.rdma_args.queue_size = queue_size;

    ASSERT_EQ(conn_rx->configure(ctx, rx_request, rx_dev_handle),
              connection::Result::success);
    ASSERT_EQ(conn_rx->establish(ctx), connection::Result::success);

    // Set up RDMA Tx
    mcm_conn_param tx_request = {};
    tx_request.type = is_tx;
    tx_request.local_addr = {.ip = "192.168.1.21", .port = "8002"};
    tx_request.remote_addr = {.ip = "192.168.1.22", .port = "8002"};
    tx_request.payload_args.rdma_args.transfer_size = payload_size;
    tx_request.payload_args.rdma_args.queue_size = queue_size;

    ASSERT_EQ(conn_tx->configure(ctx, tx_request, tx_dev_handle),
              connection::Result::success);
    ASSERT_EQ(conn_tx->establish(ctx), connection::Result::success);

    // Configure Test Receiver
    ASSERT_EQ(test_rx->configure(ctx), connection::Result::success);
    ASSERT_EQ(test_rx->establish(ctx), connection::Result::success);

    // Configure Emulated Transmitter
    ASSERT_EQ(emulated_tx->configure(ctx), connection::Result::success);
    ASSERT_EQ(emulated_tx->establish(ctx), connection::Result::success);

    // Link connections
    conn_rx->set_link(ctx, test_rx);
    emulated_tx->set_link(ctx, conn_tx);

    keep_running = true;
}

void CleanupRdmaConnections() {
    keep_running = false;
    ASSERT_EQ(conn_rx->shutdown(ctx), connection::Result::success);
    ASSERT_EQ(conn_tx->shutdown(ctx), connection::Result::success);
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    delete conn_rx;
    delete conn_tx;
    delete test_rx;
    delete emulated_tx;
}

    void SetUp() override
    {

    }

    void TearDown() override
    {

    }
};

TEST_F(RdmaRealEndpointsTest, ConcurrentTransmissionAndReception)
{
    SetupRdmaConnections(18,16);
    
    const char *test_data = "Hello RDMA World!";
    size_t data_size = strlen(test_data) + 1;

    // Transmitter thread
    std::thread transmitter_thread([&]() {
        for (int i = 0; i < 5 && keep_running; ++i) {
            ////log::info("Transmitting data")("iteration", i + 1);
            EXPECT_EQ(emulated_tx->transmit_plaintext(ctx, test_data, data_size),
                      connection::Result::success);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        keep_running = false;
    });

    // Wait for data to be received
    test_rx->wait_for_data();

    ASSERT_EQ(test_rx->received_data, std::string(test_data) + '\0')
        << "Data received does not match transmitted data.";

    transmitter_thread.join();

    // Cleanup RDMA connections
    CleanupRdmaConnections();
}

TEST_F(RdmaRealEndpointsTest, LatencyAndBandwidthForVaryingPayloadSizesAndQueueSizes) {
    const size_t payload_sizes[] = {1 << 20, 8 << 20, 3840 * 2160 * 4}; // 1 MB, 8 MB, ~32 MB
    const int queue_sizes[] = {1, 8, 32}; // Queue sizes to test
    const size_t total_data_size = 16L * 1024 * 1024 * 1024; // 16 GB
    const size_t num_iterations = 1000;
    const char filler = 'A'; // Filler character for payload

    // Store results for latency and bandwidth
    std::vector<std::tuple<double, size_t, double, double>> results; // Payload Size (MB), Queue Size, Latency (ms), Bandwidth (GB/s)

    for (size_t queue_size : queue_sizes) {
        for (size_t payload_size : payload_sizes) {
            // Setup RDMA connections with current payload size and queue size
            SetupRdmaConnections(payload_size, queue_size);

            std::vector<char> test_data(payload_size, filler);

            // Measure latency for a single transmission
            double total_latency_ms = 0.0;

            for (size_t i = 0; i < num_iterations; ++i) {
                auto start = std::chrono::high_resolution_clock::now();

                ASSERT_EQ(emulated_tx->transmit_plaintext(ctx, test_data.data(), payload_size), connection::Result::success);
                test_rx->wait_for_data(); // Wait for receiver to acknowledge the data

                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = end - start;

                total_latency_ms += elapsed.count();
            }

            // Calculate average latency per operation
            double avg_latency_ms = total_latency_ms / num_iterations;

            // Measure bandwidth
            size_t num_bandwidth_iterations = total_data_size / payload_size;
            auto start = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < num_bandwidth_iterations; ++i) {
                ASSERT_EQ(emulated_tx->transmit_plaintext(ctx, test_data.data(), payload_size), connection::Result::success);
                test_rx->wait_for_data(); // Wait for receiver to acknowledge the data
            }

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = end - start;

            // Calculate transfer rate
            double transfer_rate_gbps = (static_cast<double>(payload_size) * num_bandwidth_iterations) / (elapsed.count() * 1024 * 1024 * 1024);

            // Store results (convert payload size to MB)
            results.emplace_back(static_cast<double>(payload_size) / (1024 * 1024), queue_size, avg_latency_ms, transfer_rate_gbps);

            // Cleanup RDMA connections
            CleanupRdmaConnections();
        }
    }

    // Display results in a tabular format
    std::cout << "\n+-------------------+-------------+--------------------+----------------------+" << std::endl;
    std::cout << "| Payload Size (MB) | Queue Size  |    Latency (ms)    | Transfer Rate (GB/s) |" << std::endl;
    std::cout << "+-------------------+-------------+--------------------+----------------------+" << std::endl;
    for (const auto& result : results) {
        std::cout << "| " << std::setw(17) << std::fixed << std::setprecision(2) << std::get<0>(result)
                  << " | " << std::setw(11) << std::get<1>(result)
                  << " | " << std::setw(18) << std::fixed << std::setprecision(3) << std::get<2>(result)
                  << " | " << std::setw(20) << std::fixed << std::setprecision(3) << std::get<3>(result)
                  << " |" << std::endl;
    }
    std::cout << "+-------------------+-------------+--------------------+----------------------+" << std::endl;
}


} //connection

} //mesh