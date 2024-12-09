#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mesh/conn_rdma_rx.h"
#include "mesh/conn_rdma_tx.h"
#include "conn_rdma_test_mocks.h"
#include "libfabric_ep.h"
#include "libfabric_dev.h"
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

#define DUMMY_DATA1 "DUMMY_DATA1"
#define DUMMY_DATA2 "DUMMY_DATA2"

using namespace mesh;
using namespace mesh::log;

// Helper to configure RdmaRx
void ConfigureRdmaRx(MockRdmaRx* conn_rx, context::Context& ctx, size_t transfer_size)
{
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = transfer_size;
    request.payload_args.rdma_args.queue_size = 32;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx* dev_handle = nullptr;

    auto res = conn_rx->configure(ctx, request, dev_port, dev_handle);
    ASSERT_EQ(res, connection::Result::success) << "Failed to configure RdmaTx";
    ASSERT_EQ(conn_rx->state(), connection::State::configured) << "RdmaTx not in configured state";
}

class EmulatedTransmitter : public connection::Connection {
  public:
    uint32_t last_sent_size = 0;
    std::vector<char> last_sent_data;

    EmulatedTransmitter(context::Context &ctx)
    {
        _kind = connection::Kind::transmitter;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context &ctx) override
    {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context &ctx) override
    {
        set_state(ctx, connection::State::closed);
        return connection::Result::success;
    }

    connection::Result transmit_wrapper(context::Context &ctx, void *ptr, uint32_t sz)
    {
        // Store transmitted data for verification
        last_sent_size = sz;
        last_sent_data.assign(static_cast<char *>(ptr), static_cast<char *>(ptr) + sz);

        // Trigger RDMA Tx transmission through the transmit method
        return transmit(ctx, ptr, sz);
    }
};

// Emulated receiver class
class EmulatedReceiver : public connection::Connection {
  public:
    uint32_t received_packets = 0;
    std::string last_received_data;

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
        set_state(ctx, connection::State::closed);
        return connection::Result::success;
    }

    connection::Result on_receive(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent)
    {
        last_received_data.assign(static_cast<uint8_t *>(ptr), static_cast<uint8_t *>(ptr) + sz);
        received_packets++;
        return connection::Result::success;
    }
};

// Test Fixture
class RdmaRxTest : public ::testing::Test {
  protected:
    void SetUp() override {
        mock_ep_ops = new MockLibfabricEpOps();   // Initialize mock object for EpOps
        mock_dev_ops = new MockLibfabricDevOps(); // Initialize mock object for DevOps
        mock_conn_rx = new MockRdmaRx();        // Initialize mock object for RdmaRxTx
        SetUpMockEpOps();                         // Set up mocked functions for EpOps
        SetUpMockDevOps();                        // Set up mocked functions for DevOps
        ctx = context::WithCancel(context::Background());
        conn_rx = mock_conn_rx;                   // Assign mock to conn_rx
    }

    void TearDown() override {
        delete conn_rx;
        delete mock_ep_ops;
        delete mock_dev_ops;
        mock_ep_ops = nullptr;
        mock_dev_ops = nullptr;
        mock_conn_rx = nullptr;
    }

    context::Context ctx;
    connection::RdmaRx *conn_rx;
    MockRdmaRx *mock_conn_rx;
};

TEST_F(RdmaRxTest, EstablishSuccess)
{
    libfabric_ctx mock_dev_handle; // Mocked device handle

    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(&mock_dev_handle),
                                   ::testing::Return(0))); // Mock successful RDMA initialization

    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
            *ep_ctx = new ep_ctx_t();
            (*ep_ctx)->stop_flag = false;
            (*ep_ctx)->ep = reinterpret_cast<fid_ep *>(0xdeadbeef); // Mock endpoint
            return 0;                                               // Success
        });

    EXPECT_CALL(*mock_ep_ops, ep_recv_buf(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0)); // Simulate successful call

    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0)); // Mock successful buffer registration

    EXPECT_CALL(*mock_ep_ops, ep_destroy(::testing::_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0;
    });

    EXPECT_CALL(*mock_conn_rx, start_threads(::testing::_))
        .WillOnce(::testing::Return(connection::Result::success));

    // Configure and establish the connection
    ConfigureRdmaRx(static_cast<MockRdmaRx*>(conn_rx), ctx, 1024);

    // Act: Call on_establish
    connection::Result result = conn_rx->establish(ctx);

    // Assert
    EXPECT_EQ(result, connection::Result::success);
    EXPECT_EQ(conn_rx->state(), connection::State::active);
}

TEST_F(RdmaRxTest, EstablishFailureEpInit)
{
    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::Return(0)); // Mock successful RDMA initialization

    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(-1)); // Mock failure in ep_init

    // Configure Rdma
    ConfigureRdmaRx(static_cast<MockRdmaRx*>(conn_rx), ctx, 1024);

    // Act: Call on_establish
    auto result = conn_rx->establish(ctx);

    // Assert
    EXPECT_EQ(result, connection::Result::error_initialization_failed);
    EXPECT_EQ(conn_rx->state(), connection::State::closed);
}

TEST_F(RdmaRxTest, EstablishFailureBufferAllocation)
{
    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::Return(0)); // Mock successful RDMA initialization

    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
            *ep_ctx = new ep_ctx_t();
            (*ep_ctx)->ep = reinterpret_cast<fid_ep *>(0xdeadbeef); // Mock endpoint
            return 0;
        });

    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(-1)); // Mock failure in buffer registration

    EXPECT_CALL(*mock_ep_ops, ep_destroy(::testing::_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0;
    });

    // Configure Rdma
    ConfigureRdmaRx(static_cast<MockRdmaRx*>(conn_rx), ctx, 1024);

    // Act: Call on_establish
    auto result = conn_rx->establish(ctx);

    // Assert
    EXPECT_EQ(result, connection::Result::error_memory_registration_failed);
    EXPECT_EQ(conn_rx->state(), connection::State::closed);
}

TEST_F(RdmaRxTest, EstablishAlreadyInitialized)
{
    libfabric_ctx mock_dev_handle; // Mocked device handle

    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(&mock_dev_handle),
                                   ::testing::Return(0))); // Mock successful RDMA initialization

    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
            *ep_ctx = new ep_ctx_t();
            (*ep_ctx)->ep = reinterpret_cast<fid_ep *>(0xdeadbeef); // Mock endpoint
            return 0;
        });

    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0)); // Mock successful buffer registration

    EXPECT_CALL(*mock_ep_ops, ep_destroy(::testing::_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0;
    });

    // Configure and establish the connection
    ConfigureRdmaRx(static_cast<MockRdmaRx*>(conn_rx), ctx, 1024);

    // Establish the first time
    auto first_result = conn_rx->establish(ctx);
    EXPECT_EQ(first_result, connection::Result::success);
    EXPECT_EQ(conn_rx->state(), connection::State::active);

    // Act: Call on_establish again
    auto second_result = conn_rx->establish(ctx);

    // Assert
    EXPECT_EQ(second_result, connection::Result::error_wrong_state);
    EXPECT_EQ(conn_rx->state(), connection::State::active);
}

TEST_F(RdmaRxTest, ValidateStateTransitions)
{
    // Mock RDMA device initialization
    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::Return(0)); // Mock successful RDMA initialization

    // Mock endpoint initialization
    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
            *ep_ctx = new ep_ctx_t();
            (*ep_ctx)->ep = reinterpret_cast<fid_ep *>(0xdeadbeef); // Mock endpoint
            return 0;                                               // Success
        });

    // Mock buffer registration
    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0)); // Success

    // Mock endpoint destruction
    EXPECT_CALL(*mock_ep_ops, ep_destroy(::testing::_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0;
    });

    // Prepare arguments for configure
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = 1024 * 1024; // 1 MB transfer size

    std::string dev_port = "0000:31:00.0"; // Mocked device port
    libfabric_ctx *dev_handle = nullptr;   // Mocked device handle

    // Initial state should be not_configured
    ASSERT_EQ(conn_rx->state(), connection::State::not_configured);

    // Transition: not_configured -> configured
    ConfigureRdmaRx(static_cast<MockRdmaRx*>(conn_rx), ctx, 1024);

    // Transition: configured -> active
    connection::Result res = conn_rx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_rx->state(), connection::State::active);

    // Transition: active -> suspended
    res = conn_rx->suspend(ctx);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_rx->state(), connection::State::suspended);

    // Transition: suspended -> active
    res = conn_rx->resume(ctx);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_rx->state(), connection::State::active);

    // Transition: active -> closed
    res = conn_rx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_rx->state(), connection::State::closed);
}