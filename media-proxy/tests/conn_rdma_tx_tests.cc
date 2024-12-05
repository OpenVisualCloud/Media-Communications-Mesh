#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mesh/conn_rdma_rx.h"
#include "mesh/conn_rdma_tx.h"
#include "libfabric_ep.h"
#include "libfabric_dev.h"
#include "conn_rdma_test_mocks.h"
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

// Helper to configure RdmaTx
void ConfigureRdmaTx(connection::RdmaTx* conn_tx, context::Context& ctx, size_t transfer_size)
{
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = transfer_size;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx* dev_handle = nullptr;

    auto res = conn_tx->configure(ctx, request, dev_port, dev_handle);
    ASSERT_EQ(res, connection::Result::success) << "Failed to configure RdmaTx";
    ASSERT_EQ(conn_tx->state(), connection::State::configured) << "RdmaTx not in configured state";
}

class EmulatedTransmitter : public connection::Connection {
  public:
    uint32_t last_sent_size = 0;
    std::vector<char> last_sent_data;

    EmulatedTransmitter(context::Context &ctx)
    {
        _kind = connection::Kind::transmitter;
        set_state(ctx, connection::State::configured);
        last_sent_size = 0; // Initialize all members
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
class RdmaTxTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        mock_ep_ops = new MockLibfabricEpOps();   // Initialize mock object for EpOps
        mock_dev_ops = new MockLibfabricDevOps(); // Initialize mock object for DevOps
        SetUpMockEpOps();                         // Set up mocked functions for EpOps
        SetUpMockDevOps();                        // Set up mocked functions for DevOps
        ctx = context::WithCancel(context::Background());
        conn_tx = new connection::RdmaTx();
    }

    void TearDown() override
    {
        delete conn_tx;
        delete mock_ep_ops;
        delete mock_dev_ops;
        mock_ep_ops = nullptr;
        mock_dev_ops = nullptr;
    }

    context::Context ctx;
    connection::RdmaTx *conn_tx;
};

TEST_F(RdmaTxTest, EstablishSuccess)
{
    libfabric_ctx mock_dev_handle; // Mocked device handle

    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(&mock_dev_handle),
                                   ::testing::Return(0))); // Mock successful RDMA initialization

    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
            *ep_ctx = new ep_ctx_t();
            (*ep_ctx)->ep = reinterpret_cast<fid_ep *>(0xdeadbeef); // Mock endpoint
            return 0;                                               // Success
        });

    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0)); // Mock successful buffer registration

    EXPECT_CALL(*mock_ep_ops, ep_destroy(::testing::_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0;
    });

    // Configure and establish the connection
    ConfigureRdmaTx(conn_tx, ctx, 1024);

    // Act: Call establish
    auto result = conn_tx->establish(ctx);

    // Assert
    EXPECT_EQ(result, connection::Result::success);
    EXPECT_EQ(conn_tx->state(), connection::State::active);
}

TEST_F(RdmaTxTest, EstablishFailureEpInit)
{
   EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::Return(0)); // Mock successful RDMA initialization

    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(-1)); // Mock failure in ep_init

    // Configure and establish the connection
    ConfigureRdmaTx(conn_tx, ctx, 1024);

    // Act: Call establish
    auto result = conn_tx->establish(ctx);

    // Assert
    EXPECT_EQ(result, connection::Result::error_initialization_failed);
    EXPECT_EQ(conn_tx->state(), connection::State::closed);
}

TEST_F(RdmaTxTest, EstablishFailureBufferAllocation)
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

    // Configure and establish the connection
    ConfigureRdmaTx(conn_tx, ctx, 1024);

    // Act: Call establish
    auto result = conn_tx->establish(ctx);

    // Assert
    EXPECT_EQ(result, connection::Result::error_memory_registration_failed);
    EXPECT_EQ(conn_tx->state(), connection::State::closed);
}

TEST_F(RdmaTxTest, EstablishAlreadyInitialized) {
    libfabric_ctx mock_dev_handle;

    // Mock RDMA initialization
    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(&mock_dev_handle),
            ::testing::Return(0)));

    // Mock endpoint initialization
    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce([](ep_ctx_t** ep_ctx, ep_cfg_t* cfg) -> int {
            *ep_ctx = new ep_ctx_t();
            (*ep_ctx)->ep = reinterpret_cast<fid_ep*>(0xdeadbeef);
            return 0;
        });

    // Mock buffer registration
    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));

    // Mock endpoint destruction
    EXPECT_CALL(*mock_ep_ops, ep_destroy(::testing::_))
        .WillOnce([](ep_ctx_t** ep_ctx) -> int {
            delete *ep_ctx;
            *ep_ctx = nullptr;
            return 0;
        });

    // Configure and establish the connection
    ConfigureRdmaTx(conn_tx, ctx, 1024);

    // First establish
    auto first_result = conn_tx->establish(ctx);
    EXPECT_EQ(first_result, connection::Result::success);
    EXPECT_EQ(conn_tx->state(), connection::State::active);

    // Second establish
    auto second_result = conn_tx->establish(ctx);
    EXPECT_EQ(second_result, connection::Result::error_wrong_state);
    EXPECT_EQ(conn_tx->state(), connection::State::active);
}


TEST_F(RdmaTxTest, ValidateStateTransitions)
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
    ASSERT_EQ(conn_tx->state(), connection::State::not_configured);

    // Transition: not_configured -> configured
    auto res = conn_tx->configure(ctx, request, dev_port, dev_handle);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Transition: configured -> active
    res = conn_tx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_tx->state(), connection::State::active);

    // Transition: active -> suspended
    res = conn_tx->suspend(ctx);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_tx->state(), connection::State::suspended);

    // Transition: suspended -> active
    res = conn_tx->resume(ctx);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_tx->state(), connection::State::active);

    // Transition: active -> closed
    res = conn_tx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success);
    ASSERT_EQ(conn_tx->state(), connection::State::closed);
}