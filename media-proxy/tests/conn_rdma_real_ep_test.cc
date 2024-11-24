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
        log::info("Data received")("size", sz)("data", static_cast<char *>(ptr));
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
        setLogLevel(log_level);
        std::lock_guard<std::mutex> lock(mtx);
        received_data = std::string(static_cast<char *>(ptr), sz);
        data_received = true;
        cv.notify_all();
        log::info("Data received via TestReceiver")("size", sz)("data", received_data);
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

    void SetUp() override
    {
        ctx = context::WithCancel(context::Background());

        conn_rx = new RdmaRx();
        conn_tx = new RdmaTx();
        test_rx = new TestReceiver(ctx);
        emulated_tx = new EmulatedTransmitter(ctx);

        libfabric_ctx *rx_dev_handle = NULL;
        libfabric_ctx *tx_dev_handle = NULL;

        // Set up RDMA Rx
        mcm_conn_param rx_request = {};
        rx_request.type = is_rx;
        rx_request.local_addr = {.ip = "192.168.1.20", .port = "8002"};
        rx_request.remote_addr = {.ip = "192.168.1.25", .port = "8003"};
        rx_request.payload_args.rdma_args.transfer_size = 18;

        ASSERT_EQ(conn_rx->configure(ctx, rx_request, "0000:31:00.1", rx_dev_handle),
                  connection::Result::success);
        ASSERT_EQ(conn_rx->establish(ctx), connection::Result::success);

        // Set up RDMA Tx
        mcm_conn_param tx_request = {};
        tx_request.type = is_tx;
        tx_request.local_addr = {.ip = "192.168.1.25", .port = "8003"};
        tx_request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
        tx_request.payload_args.rdma_args.transfer_size = 18;

        ASSERT_EQ(conn_tx->configure(ctx, tx_request, "0000:ca:00.1", tx_dev_handle),
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

    void TearDown() override
    {
        keep_running = false;
        ASSERT_EQ(conn_rx->shutdown(ctx), connection::Result::success);
        ASSERT_EQ(conn_tx->shutdown(ctx), connection::Result::success);
        delete conn_rx;
        delete conn_tx;
        delete test_rx;
        delete emulated_tx;
    }
};

TEST_F(RdmaRealEndpointsTest, ConcurrentTransmissionAndReception)
{
    setLogLevel(log_level);
    
    const char *test_data = "Hello RDMA World!";
    size_t data_size = strlen(test_data) + 1;

    // Transmitter thread
    std::thread transmitter_thread([&]() {
        for (int i = 0; i < 5 && keep_running; ++i) {
            log::info("Transmitting data")("iteration", i + 1);
            EXPECT_EQ(emulated_tx->transmit_plaintext(ctx, test_data, data_size),
                      connection::Result::success);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        keep_running = false;
    });

    // Wait for data to be received
    test_rx->wait_for_data();

    ASSERT_EQ(test_rx->received_data, std::string(test_data) + '\0')
        << "Data received does not match transmitted data.";

    transmitter_thread.join();
}
} // namespace connection
} // namespace mesh
