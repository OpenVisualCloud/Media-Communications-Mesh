#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "libfabric_ep.h"
#include "libfabric_dev.h"
#include "mesh/conn_rdma.h"
#include "conn_rdma_test_mocks.h"

using namespace testing;
using namespace mesh;
using namespace connection;
using namespace mesh::log;

class TestRdma : public Rdma {
  public:
    using Rdma::add_to_queue;
    using Rdma::cleanup_queue;
    using Rdma::cleanup_resources;
    using Rdma::configure;
    using Rdma::configure_endpoint;
    using Rdma::consume_from_queue;
    using Rdma::ep_cfg;
    using Rdma::ep_ctx;
    using Rdma::init;
    using Rdma::init_queue_with_elements;
    using Rdma::mDevHandle;
    using Rdma::on_delete;
    using Rdma::on_establish;
    using Rdma::on_shutdown;
    using Rdma::shutdown_rdma;
    using Rdma::trx_sz;
};

// Test fixture
class RdmaTest : public ::testing::Test {
  protected:
    TestRdma *rdma;
    context::Context ctx;

    void SetUp() override
    {
        mock_ep_ops = new MockLibfabricEpOps();
        mock_dev_ops = new MockLibfabricDevOps();
        SetUpMockEpOps();
        SetUpMockDevOps();
        ctx = context::WithCancel(context::Background());
        rdma = new TestRdma();
    }

    void TearDown() override
    {
        delete rdma;
        delete mock_ep_ops;
        delete mock_dev_ops;
        mock_ep_ops = nullptr;
        mock_dev_ops = nullptr;
    }

    void ConfigureRdma(size_t transfer_size)
    {
        mcm_conn_param request = {};
        request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
        request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
        request.payload_args.rdma_args.transfer_size = transfer_size;

        std::string dev_port = "0000:31:00.0";
        libfabric_ctx *dev_handle = nullptr;

        EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
            .WillOnce(
                ::testing::DoAll(::testing::SetArgPointee<0>(dev_handle),
                                 ::testing::Return(0))); // Mock successful RDMA initialization

        auto res =
            rdma->configure(ctx, request, dev_port, dev_handle, Kind::transmitter, direction::TX);
        ASSERT_EQ(res, Result::success);
        ASSERT_EQ(rdma->state(), State::configured);
    }
};

TEST_F(RdmaTest, ConfigureSuccess)
{
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = 1024;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx *dev_handle = nullptr;

    // Call configure with a null dev_handle to trigger rdma_init in on_establish
    auto res = rdma->configure(ctx, request, dev_port, dev_handle, Kind::receiver, direction::RX);
    ASSERT_EQ(res, Result::success);
    ASSERT_EQ(rdma->state(), State::configured);
}

TEST_F(RdmaTest, EstablishSuccess)
{

    ConfigureRdma(1024);

    EXPECT_CALL(*mock_ep_ops, ep_init(_, _)).WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
        *ep_ctx = new ep_ctx_t();
        return 0;
    });

    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(_, _, _)).WillRepeatedly(Return(0));

    EXPECT_CALL(*mock_ep_ops, ep_destroy(_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        return 0;
    });

    auto result = rdma->on_establish(ctx);
    EXPECT_EQ(result, Result::success);
    EXPECT_EQ(rdma->state(), State::active);
}

TEST_F(RdmaTest, EstablishFailureEpInit)
{
    ConfigureRdma(1024);

    EXPECT_CALL(*mock_ep_ops, ep_init(_, _)).WillOnce(Return(-1));

    auto result = rdma->establish(ctx);
    EXPECT_EQ(result, Result::error_initialization_failed);
    EXPECT_EQ(rdma->state(), State::closed);
}

TEST_F(RdmaTest, CleanupResources)
{
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = 1024;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx *dev_handle = nullptr;

    // Mock rdma_init
    EXPECT_CALL(*mock_dev_ops, rdma_init(_)).WillOnce([](libfabric_ctx **ctx) -> int {
        *ctx = new libfabric_ctx();
        return 0; // Success
    });

    // Mock ep_init
    EXPECT_CALL(*mock_ep_ops, ep_init(_, _)).WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
        *ep_ctx = new ep_ctx_t();
        return 0; // Success
    });

    // Mock ep_reg_mr
    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(_, _, _)).WillRepeatedly(Return(0));

    // Mock ep_destroy
    EXPECT_CALL(*mock_ep_ops, ep_destroy(_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0; // Success
    });

    // Configure and establish
    rdma->configure(ctx, request, dev_port, dev_handle, Kind::transmitter, direction::TX);
    rdma->establish(ctx);

    // Trigger cleanup
    rdma->shutdown_rdma(ctx);

    // Verify state is closed
    EXPECT_EQ(rdma->state(), State::closed);
}

TEST_F(RdmaTest, ValidateStateTransitions)
{
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = 1024;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx *dev_handle = nullptr;

    // Initial state should be `not_configured`
    ASSERT_EQ(rdma->state(), State::not_configured);

    // Mock rdma_init
    EXPECT_CALL(*mock_dev_ops, rdma_init(_)).WillOnce([](libfabric_ctx **ctx) -> int {
        *ctx = new libfabric_ctx();
        return 0; // Success
    });

    // Mock ep_init
    EXPECT_CALL(*mock_ep_ops, ep_init(_, _)).WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
        *ep_ctx = new ep_ctx_t();
        return 0; // Success
    });
    // Mock ep_reg_mr
    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(_, _, _)).WillRepeatedly(Return(0));

    // Mock ep_destroy
    EXPECT_CALL(*mock_ep_ops, ep_destroy(_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0; // Success
    });

    // Transition: `not_configured` -> `configured`
    auto res =
        rdma->configure(ctx, request, dev_port, dev_handle, Kind::transmitter, direction::TX);
    ASSERT_EQ(res, Result::success);
    ASSERT_EQ(rdma->state(), State::configured);

    // Transition: `configured` -> `active`
    res = rdma->establish(ctx);
    ASSERT_EQ(res, Result::success);
    ASSERT_EQ(rdma->state(), State::active);

    // Transition: `active` -> `suspended`
    res = rdma->suspend(ctx);
    ASSERT_EQ(res, Result::success);
    ASSERT_EQ(rdma->state(), State::suspended);

    // Transition: `suspended` -> `active`
    res = rdma->resume(ctx);
    ASSERT_EQ(res, Result::success);
    ASSERT_EQ(rdma->state(), State::active);

    // Transition: `active` -> `closed`
    rdma->shutdown_rdma(ctx);
    ASSERT_EQ(rdma->state(), State::closed);
}

TEST_F(RdmaTest, ValidateKindAndDirectionRX)
{
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = 1024;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx *dev_handle = nullptr;

    // Case 1: Receiver configuration
    auto res = rdma->configure(ctx, request, dev_port, dev_handle, Kind::receiver, direction::RX);
    ASSERT_EQ(res, Result::success);
    ASSERT_EQ(rdma->state(), State::configured);
    EXPECT_EQ(rdma->get_kind(), Kind::receiver);
    EXPECT_EQ(rdma->ep_cfg.dir, direction::RX);
}

TEST_F(RdmaTest, ValidateKindAndDirectionTX)
{
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = 1024;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx *dev_handle = nullptr;

    // Case 2: Transmitter configuration
    auto res =
        rdma->configure(ctx, request, dev_port, dev_handle, Kind::transmitter, direction::TX);
    ASSERT_EQ(res, Result::success);
    ASSERT_EQ(rdma->state(), State::configured);
    EXPECT_EQ(rdma->get_kind(), Kind::transmitter);
    EXPECT_EQ(rdma->ep_cfg.dir, direction::TX);
}

TEST_F(RdmaTest, InvalidDirectionForKind)
{
    mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = 1024;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx *dev_handle = nullptr;

    // Invalid case: Receiver with Send direction
    auto res = rdma->configure(ctx, request, dev_port, dev_handle, Kind::receiver, direction::TX);
    EXPECT_NE(res, Result::success);

    // Invalid case: Transmitter with Receive direction
    res = rdma->configure(ctx, request, dev_port, dev_handle, Kind::transmitter, direction::RX);
    EXPECT_NE(res, Result::success);
}

TEST_F(RdmaTest, InitQueueWithElementsSuccess) {
    // Parameters for initialization
    size_t capacity = 5;
    size_t trx_sz = 1024;

    // Step 1: Initialize the RDMA queue
    auto result = rdma->init_queue_with_elements(capacity, trx_sz);
    ASSERT_EQ(result, Result::success); // Verify successful initialization
    ASSERT_EQ(rdma->get_buffer_queue_size(), capacity); // Check initial queue size

    // Step 2: Validate buffer allocation and queue operations
    for (size_t i = 0; i < capacity; ++i) {
        void* buf = nullptr;

        // Consume a buffer from the queue
        auto res = rdma->consume_from_queue(ctx, &buf);
        ASSERT_EQ(res, Result::success); // Ensure successful consumption
        ASSERT_NE(buf, nullptr);         // Check that the buffer is valid

        // Check if the buffer is zero-initialized
        char* data = static_cast<char*>(buf);
        for (size_t j = 0; j < trx_sz; ++j) {
            ASSERT_EQ(data[j], 0); // Verify zero-initialization
        }

        // Add the buffer back to the queue
        res = rdma->add_to_queue(buf);
        ASSERT_EQ(res, Result::success); // Ensure successful addition
    }

    // Step 3: Verify the queue size is restored
    ASSERT_EQ(rdma->get_buffer_queue_size(), capacity); // Check restored queue size
}


TEST_F(RdmaTest, InitQueueWithElementsFailureMemoryAllocationWithSize0) {
    size_t capacity = 10;
    size_t trx_sz = 0; // Invalid size to simulate failure

    auto result = rdma->init_queue_with_elements(capacity, trx_sz);
    ASSERT_EQ(result, Result::error_bad_argument);
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, InitQueueWithElementsFailureMemoryAllocationWithSize1GB) {
    size_t capacity = 10;
    size_t trx_sz = 1<<31; // Invalid size to simulate failure

    auto result = rdma->init_queue_with_elements(capacity, trx_sz);
    ASSERT_EQ(result, Result::error_bad_argument);
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, AddToQueueSuccess) {
    int dummy_data = 42;

    auto result = rdma->add_to_queue(&dummy_data);
    ASSERT_EQ(result, Result::success);
    ASSERT_EQ(rdma->get_buffer_queue_size(), 1);

    // Validate the added element
    void *element = nullptr;
    auto res = rdma->consume_from_queue(ctx, &element);
    ASSERT_EQ(res, Result::success);
    ASSERT_EQ(element, &dummy_data);
}

TEST_F(RdmaTest, AddToQueueFailureNullptr) {
    auto result = rdma->add_to_queue(nullptr);
    ASSERT_EQ(result, Result::error_bad_argument);
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, ConsumeFromQueueSuccess) {
    int dummy_data1 = 42;
    int dummy_data2 = 84;

    rdma->add_to_queue(&dummy_data1);
    rdma->add_to_queue(&dummy_data2);

    // Consume first element
    void *element = nullptr;
    auto result = rdma->consume_from_queue(ctx, &element);
    ASSERT_EQ(result, Result::success);
    ASSERT_EQ(element, &dummy_data1);

    // Consume second element
    result = rdma->consume_from_queue(ctx, &element);
    ASSERT_EQ(result, Result::success);
    ASSERT_EQ(element, &dummy_data2);

    // Queue should be empty now
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, ConsumeFromQueueFailureEmptyQueue) {
    void *element = nullptr;
    auto result = rdma->consume_from_queue(ctx, &element);
    ASSERT_EQ(result, Result::error_no_buffer);
    ASSERT_EQ(element, nullptr);
}

TEST_F(RdmaTest, ConsumeFromQueueFailureContextCancelled) {
    int dummy_data = 42;
    rdma->add_to_queue(&dummy_data);

    // Cancel the context
    ctx.cancel();

    void *element = nullptr;
    auto result = rdma->consume_from_queue(ctx, &element);
    ASSERT_EQ(result, Result::error_operation_cancelled);
    ASSERT_EQ(element, nullptr);

    // The queue should remain unchanged
    ASSERT_EQ(rdma->get_buffer_queue_size(), 1);
}

TEST_F(RdmaTest, CleanupQueue) {
    size_t capacity = 5;
    size_t trx_sz = 1024;

    // Initialize the queue with elements
    rdma->init_queue_with_elements(capacity, trx_sz);

    // Ensure the queue is not empty
    ASSERT_EQ(rdma->get_buffer_queue_size(), capacity);

    // Cleanup the queue
    rdma->cleanup_queue();

    // Validate that the queue is now empty
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, QueuePerformanceTest) {
    const size_t iterations = 100000;
    const size_t trx_sz = 1024;

    // Initialize queue
    rdma->init_queue_with_elements(iterations, trx_sz);

    // Measure time taken for queue operations
    auto start = std::chrono::high_resolution_clock::now();

    // Consume from queue
    for (size_t i = 0; i < iterations; ++i) {
        void *buf = nullptr;
        rdma->consume_from_queue(ctx, &buf);
        std::free(buf);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Processed " << iterations << " queue operations in " << elapsed.count() << " seconds.\n";

    // Ensure queue is empty
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, RepeatedShutdown) {
        mcm_conn_param request = {};
    request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
    request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
    request.payload_args.rdma_args.transfer_size = 1024;

    std::string dev_port = "0000:31:00.0";
    libfabric_ctx *dev_handle = nullptr;

    // Mock rdma_init
    EXPECT_CALL(*mock_dev_ops, rdma_init(_)).WillOnce([](libfabric_ctx **ctx) -> int {
        *ctx = new libfabric_ctx();
        return 0; // Success
    });

    // Mock ep_init
    EXPECT_CALL(*mock_ep_ops, ep_init(_, _)).WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
        *ep_ctx = new ep_ctx_t();
        return 0; // Success
    });

    // Mock ep_reg_mr
    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(_, _, _)).WillRepeatedly(Return(0));

    // Mock ep_destroy
    EXPECT_CALL(*mock_ep_ops, ep_destroy(_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0; // Success
    });

    // Configure and establish
    rdma->configure(ctx, request, dev_port, dev_handle, Kind::transmitter, direction::TX);
    rdma->establish(ctx);

    for (int i = 0; i++; i < 10) {
        // Repeated shutdown should not crash
        rdma->shutdown_rdma(ctx);
        ASSERT_EQ(rdma->state(), State::closed);
    }
}

TEST_F(RdmaTest, StressTest) {
    const size_t num_threads = 16;
    const size_t operations_per_thread = 10000;

    std::vector<std::thread> threads;

    // Initialize queue with a large number of elements
    rdma->init_queue_with_elements(num_threads * operations_per_thread, sizeof(int));

    // Launch threads to perform concurrent queue operations
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (size_t j = 0; j < operations_per_thread; ++j) {
                void *buf = nullptr;
                if (j % 2 == 0) {
                    buf = std::calloc(1, sizeof(int));
                    rdma->add_to_queue(buf);
                } else {
                    if (rdma->consume_from_queue(ctx, &buf) == Result::success) {
                        std::free(buf);
                    }
                }
            }
        });
    }

    // Wait for all threads to finish
    for (auto &thread : threads) {
        thread.join();
    }
    ////log::debug("Buffer state at the end of test")("queue_size", rdma->get_buffer_queue_size());
    // Ensure the queue is empty
    rdma->cleanup_queue();
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, ConcurrentAccessWithDelays)
{
    const size_t num_threads = 4;
    const size_t operations_per_thread = 1000;

    std::vector<std::thread> threads;

    // Initialize queue with elements
    rdma->init_queue_with_elements(num_threads * operations_per_thread, sizeof(int));

    // Clear the queue elements safely before concurrent access
    void *buf = nullptr;
    while (rdma->consume_from_queue(ctx, &buf) == Result::success) {
        std::free(buf);
    }

    // Ensure the queue is empty
    ASSERT_TRUE(rdma->is_buffer_queue_empty());

    // Start concurrent operations
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (size_t j = 0; j < operations_per_thread; ++j) {
                void *thread_buf = nullptr; // Thread-local buffer
                if (j % 2 == 0) {
                    // Allocate memory and add to queue
                    thread_buf = std::calloc(1, sizeof(int));
                    rdma->add_to_queue(thread_buf);
                    std::this_thread::sleep_for(std::chrono::microseconds(10)); // Simulate delay
                } else {
                    // Consume memory and free it
                    if (rdma->consume_from_queue(ctx, &thread_buf) == Result::success) {
                            std::free(thread_buf);
                    }
                    std::this_thread::sleep_for(std::chrono::microseconds(5)); // Simulate delay
                }
            }
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify the queue is empty after all operations
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}


