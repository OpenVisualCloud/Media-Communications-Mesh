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
    using Rdma::trx_sz;
    void set_kind(Kind kind) { _kind = kind; }
    Result start_threads(mesh::context::Context& ctx) {return Result::success;}
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

    void ConfigureRdma(TestRdma *rdma, context::Context& ctx, size_t transfer_size, Kind kind) {
        mcm_conn_param request = {};
        request.local_addr = {.ip = "192.168.1.10", .port = "8001"};
        request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
        request.payload_args.rdma_args.transfer_size = transfer_size;
        request.payload_args.rdma_args.queue_size = 32;

        std::string dev_port = "0000:31:00.0";
        libfabric_ctx *dev_handle = nullptr;

        rdma->set_kind(kind);
        auto res = rdma->configure(ctx, request, dev_port, dev_handle);
        ASSERT_EQ(res, Result::success);
        ASSERT_EQ(rdma->state(), State::configured);
        ASSERT_EQ(rdma->kind(), kind);
    }
};

TEST_F(RdmaTest, ConfigureSuccess)
{
    // Use ConfigureRdma helper to set up the test environment
    ConfigureRdma(rdma, ctx, 1024, Kind::receiver);

    // Verify state and kind after configuration
    ASSERT_EQ(rdma->state(), State::configured);
    ASSERT_EQ(rdma->kind(), Kind::receiver);
}

TEST_F(RdmaTest, EstablishSuccess) {

    ConfigureRdma(rdma, ctx, 1024, Kind::receiver);

    libfabric_ctx *dev_handle = nullptr;

    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(dev_handle), ::testing::Return(0)));

    EXPECT_CALL(*mock_ep_ops, ep_init(::testing::_, ::testing::_))
        .WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
            *ep_ctx = new ep_ctx_t();
            (*ep_ctx)->stop_flag = false;
            (*ep_ctx)->ep = reinterpret_cast<fid_ep *>(0xdeadbeef); // Mock endpoint
            return 0;                                               // Success
        });

    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(_, _, _)).WillRepeatedly(Return(0));

    EXPECT_CALL(*mock_ep_ops, ep_destroy(_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        return 0;
    });

    auto result = rdma->on_establish(ctx);
    ASSERT_EQ(result, Result::success);
    ASSERT_EQ(rdma->state(), State::active);
}

TEST_F(RdmaTest, EstablishFailureEpInit) {
    // Use ConfigureRdma helper to set up the test environment
    ConfigureRdma(rdma, ctx, 1024, Kind::receiver);

    libfabric_ctx *dev_handle = nullptr;

    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(dev_handle), ::testing::Return(0)));

    // Simulate failure during ep_init
    EXPECT_CALL(*mock_ep_ops, ep_init(_, _)).WillOnce(Return(-1));

    auto result = rdma->on_establish(ctx);
    ASSERT_EQ(result, Result::error_initialization_failed);
    ASSERT_EQ(rdma->state(), State::closed);
}

TEST_F(RdmaTest, CleanupResources)
{

    ConfigureRdma(rdma, ctx, 1024, Kind::receiver);

    libfabric_ctx *dev_handle = nullptr;

    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(dev_handle),
                                   ::testing::Return(0)));

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
    ASSERT_EQ(result, Result::success);

    // Trigger cleanup
    rdma->on_shutdown(ctx);

    // Verify state is closed
    EXPECT_EQ(rdma->state(), State::closed);
}

TEST_F(RdmaTest, ValidateStateTransitions)
{
    EXPECT_CALL(*mock_ep_ops, ep_init(_, _)).WillOnce([](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
        *ep_ctx = new ep_ctx_t();
        return 0; // Success
    });

    EXPECT_CALL(*mock_ep_ops, ep_destroy(_)).WillOnce([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0; // Success
    });

    EXPECT_CALL(*mock_ep_ops, ep_reg_mr(_, _, _)).WillRepeatedly(Return(0));

    libfabric_ctx *dev_handle = nullptr;

    EXPECT_CALL(*mock_dev_ops, rdma_init(::testing::_))
                .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(dev_handle),
                          ::testing::Return(0)));

    // Initial state should be `not_configured`
    ASSERT_EQ(rdma->state(), State::not_configured);

    // Transition: `not_configured` -> `configured`
    ConfigureRdma(rdma, ctx, 1024, Kind::transmitter);
    ASSERT_EQ(rdma->state(), State::configured);

    // Transition: `configured` -> `active`
    Result res = rdma->establish(ctx);
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
    rdma->on_shutdown(ctx);
    ASSERT_EQ(rdma->state(), State::closed);
}

TEST_F(RdmaTest, InitQueueWithElementsSuccess) {
    // Parameters for initialization
    const size_t capacity = 5;
    const size_t trx_sz = 1024;
    const size_t aligned_trx_sz = ((trx_sz + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    // Step 1: Initialize the RDMA queue
    auto result = rdma->init_queue_with_elements(capacity, trx_sz);
    ASSERT_EQ(result, Result::success);                 // Verify successful initialization
    ASSERT_EQ(rdma->get_buffer_queue_size(), capacity); // Check initial queue size

    // Step 2: Validate buffer allocation and queue operations
    for (size_t i = 0; i < capacity; ++i) {
        void *buf = nullptr;

        // Consume a buffer from the queue
        auto res = rdma->consume_from_queue(ctx, &buf);
        ASSERT_EQ(res, Result::success); // Ensure successful consumption
        ASSERT_NE(buf, nullptr);         // Check that the buffer is valid

        // Check if the buffer is within the allocated memory block
        ASSERT_GE(buf, rdma->get_buffer_block());
        ASSERT_LT(buf, static_cast<char *>(rdma->get_buffer_block()) + capacity * aligned_trx_sz);

        // Validate that the buffer is page-aligned
        ASSERT_EQ(reinterpret_cast<uintptr_t>(buf) % PAGE_SIZE, 0);

        // Check if the buffer is zero-initialized
        char *data = static_cast<char *>(buf);
        for (size_t j = 0; j < trx_sz; ++j) {
            ASSERT_EQ(data[j], 0); // Verify zero-initialization
        }

        // Add the buffer back to the queue
        res = rdma->add_to_queue(buf);
        ASSERT_EQ(res, Result::success); // Ensure successful addition
    }

    // Step 3: Verify the queue size is restored
    ASSERT_EQ(rdma->get_buffer_queue_size(), capacity); // Check restored queue size

    // Step 4: Cleanup resources
    rdma->cleanup_queue();
}

TEST_F(RdmaTest, InitQueueWithElementsFailureMemoryAllocationWithSize0)
{
    size_t capacity = 10;
    size_t trx_sz = 0; // Invalid size to simulate failure

    auto result = rdma->init_queue_with_elements(capacity, trx_sz);
    ASSERT_EQ(result, Result::error_bad_argument);
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, InitQueueWithElementsFailureMemoryAllocationWithSize1GB)
{
    size_t capacity = 10;
    size_t trx_sz = 1 << 31; // Invalid size to simulate failure

    auto result = rdma->init_queue_with_elements(capacity, trx_sz);
    ASSERT_EQ(result, Result::error_bad_argument);
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, AddToQueueSuccess)
{
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

TEST_F(RdmaTest, AddToQueueFailureNullptr)
{
    auto result = rdma->add_to_queue(nullptr);
    ASSERT_EQ(result, Result::error_bad_argument);
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

TEST_F(RdmaTest, ConsumeFromQueueSuccess)
{
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

TEST_F(RdmaTest, ConsumeFromQueueFailureEmptyQueue)
{
    void *element = nullptr;
    auto result = rdma->consume_from_queue(ctx, &element);
    ASSERT_EQ(result, Result::error_no_buffer);
    ASSERT_EQ(element, nullptr);
}

TEST_F(RdmaTest, ConsumeFromQueueFailureContextCancelled)
{
    int dummy_data = 42;
    rdma->add_to_queue(&dummy_data);

    // Cancel the context
    ctx.cancel();

    void *element = nullptr;
    auto result = rdma->consume_from_queue(ctx, &element);
    ASSERT_EQ(result, Result::error_context_cancelled);
    ASSERT_EQ(element, nullptr);

    // The queue should remain unchanged
    ASSERT_EQ(rdma->get_buffer_queue_size(), 1);
}

TEST_F(RdmaTest, CleanupQueue)
{
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

TEST_F(RdmaTest, QueuePerformanceTest)
{
    const size_t iterations = 1000000;
    const size_t trx_sz = 1024;

    // Initialize queue
    auto result = rdma->init_queue_with_elements(iterations, trx_sz);
    ASSERT_EQ(result, Result::success);

    size_t aligned_trx_sz = ((trx_sz + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    // Measure time taken for queue operations
    auto start = std::chrono::high_resolution_clock::now();

    // Consume from queue
    for (size_t i = 0; i < iterations; ++i) {
        void *buf = nullptr;
        auto res = rdma->consume_from_queue(ctx, &buf);
        ASSERT_EQ(res, Result::success);
        ASSERT_NE(buf, nullptr);

        // Check if the buffer is within the allocated range
        ASSERT_GE(buf, rdma->get_buffer_block()); // Buffer >= base address
        ASSERT_LT(buf, static_cast<char *>(rdma->get_buffer_block()) +
                           iterations * aligned_trx_sz); // Buffer < end address
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Processed " << iterations << " queue operations in " << elapsed.count()
              << " seconds.\n";

    // Ensure queue is empty
    ASSERT_TRUE(rdma->is_buffer_queue_empty());

    // Cleanup
    rdma->cleanup_queue();
}

TEST_F(RdmaTest, RepeatedShutdown) {
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
    EXPECT_CALL(*mock_ep_ops, ep_destroy(_)).Times(1).WillRepeatedly([](ep_ctx_t **ep_ctx) -> int {
        delete *ep_ctx;
        *ep_ctx = nullptr;
        return 0; // Success
    });

    // Use ConfigureRdma helper to configure the RDMA instance
    ConfigureRdma(rdma, ctx, 1024, Kind::transmitter);

    // Establish connection
    rdma->establish(ctx);

    for (int i = 0; i < 10; ++i) {
        // Repeated shutdown should not crash
        rdma->on_shutdown(ctx);
        ASSERT_EQ(rdma->state(), State::closed);
    }
}

TEST_F(RdmaTest, StressTest) {
    const size_t num_threads = 128;
    const size_t operations_per_thread = 10000;

    std::vector<std::thread> threads;

    // Initialize queue with a large number of elements
    const size_t capacity = num_threads * operations_per_thread;
    auto result = rdma->init_queue_with_elements(capacity, sizeof(int));
    ASSERT_EQ(result, Result::success); // Ensure initialization was successful

    // Atomic counters for tracking
    std::atomic<size_t> consumed_buffers{0};
    std::atomic<size_t> freed_buffers{0};

    // Launch threads to perform concurrent queue operations
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (size_t j = 0; j < operations_per_thread; ++j) {
                void *buf = nullptr;

                if (j % 2 == 0) {
                    // Consume from the queue and re-add the buffer
                    if (rdma->consume_from_queue(ctx, &buf) == Result::success) {
                        consumed_buffers.fetch_add(1, std::memory_order_relaxed);
                        rdma->add_to_queue(buf);
                    }
                } else {
                    // Consume from the queue and simulate processing/freeing
                    if (rdma->consume_from_queue(ctx, &buf) == Result::success) {
                        std::memset(buf, 0, sizeof(int));
                        freed_buffers.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }

    // Wait for all threads to finish
    for (auto &thread : threads) {
        thread.join();
    }

    // Cleanup remaining buffers
    rdma->cleanup_queue();

    // Verify results
    ASSERT_EQ(consumed_buffers.load() + freed_buffers.load(), capacity); // Ensure all buffers were processed
    ASSERT_TRUE(rdma->is_buffer_queue_empty()); // Ensure queue is empty
}


TEST_F(RdmaTest, ConcurrentAccessWithDelays) {
    const size_t num_threads = 128;
    const size_t operations_per_thread = 1000;

    std::vector<std::thread> threads;

    // Initialize queue with elements
    const size_t capacity = num_threads * operations_per_thread;
    auto result = rdma->init_queue_with_elements(capacity, sizeof(int));
    ASSERT_EQ(result, Result::success);

    // Start concurrent operations
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (size_t j = 0; j < operations_per_thread; ++j) {
                void *buf = nullptr; // Thread-local buffer

                if (j % 2 == 0) {
                    // Consume a buffer and return it to the queue
                    if (rdma->consume_from_queue(ctx, &buf) == Result::success) {
                        std::memset(buf, 0, sizeof(int)); // Simulate processing
                        rdma->add_to_queue(buf);
                    }
                } else {
                    // Consume a buffer and simulate processing without returning it
                    if (rdma->consume_from_queue(ctx, &buf) == Result::success) {
                        std::memset(buf, 0, sizeof(int)); // Simulate processing
                    }
                }

                // Simulate delays
                std::this_thread::sleep_for(std::chrono::microseconds(j % 2 == 0 ? 10 : 5));
            }
        });
    }

    // Wait for all threads to finish
    for (auto &thread : threads) {
        thread.join();
    }

    // Verify the queue is not empty, as half of the buffers were not returned
    ASSERT_FALSE(rdma->is_buffer_queue_empty());

    // Cleanup remaining buffers
    rdma->cleanup_queue();
    ASSERT_TRUE(rdma->is_buffer_queue_empty());
}

