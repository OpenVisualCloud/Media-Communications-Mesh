#include <gtest/gtest.h>
#include "mesh/conn_rdma.h"
#include <cstring>

using namespace mesh;
using namespace connection;

class TestRdma : public Rdma {
public:
    using Rdma::add_to_queue;
    using Rdma::consume_from_queue;
    using Rdma::init_queue_with_elements;
    using Rdma::cleanup_queue;
    using Rdma::configure_endpoint;
    using Rdma::ep_ctx;
    using Rdma::init;
    using Rdma::trx_sz;
    using Rdma::mDevHandle;
    using Rdma::ep_cfg;
    using Rdma::on_establish;
    using Rdma::on_shutdown;
    using Rdma::on_delete;
};

class RdmaTest : public ::testing::Test {
protected:
    TestRdma rdma;
    context::Context ctx;

    void SetUp() override {
    ctx = context::Context();  // Ensure a valid instance of Context is created.
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(RdmaTest, AddToQueueNullElement) {
    Result result = rdma.add_to_queue(nullptr);
    EXPECT_EQ(result, Result::error_bad_argument);
}

TEST_F(RdmaTest, AddToQueueValidElement) {
    int element = 42;
    Result result = rdma.add_to_queue(&element);
    EXPECT_EQ(result, Result::success);
}

TEST_F(RdmaTest, ConsumeFromQueueEmptyQueue) {
    void* element = nullptr;
    Result result = rdma.consume_from_queue(ctx, &element);
    EXPECT_EQ(result, Result::error_general_failure);
}

TEST_F(RdmaTest, ConsumeFromQueueValidElement) {
    int element = 42;
    rdma.add_to_queue(&element);

    void* consumed_element = nullptr;
    Result result = rdma.consume_from_queue(ctx, &consumed_element);
    EXPECT_EQ(result, Result::success);
    EXPECT_EQ(consumed_element, &element);
}

TEST_F(RdmaTest, InitQueueWithElements) {
    size_t capacity = 10;
    size_t trx_sz = 128;
    Result result = rdma.init_queue_with_elements(capacity, trx_sz);
    EXPECT_EQ(result, Result::success);
}

TEST_F(RdmaTest, CleanupQueue) {
    size_t capacity = 10;
    size_t trx_sz = 128;
    rdma.init_queue_with_elements(capacity, trx_sz);
    rdma.cleanup_queue();

    void* element = nullptr;
    Result result = rdma.consume_from_queue(ctx, &element);
    EXPECT_EQ(result, Result::error_general_failure);
}

TEST_F(RdmaTest, ConfigureEndpointNotInitialized) {
    Result result = rdma.configure_endpoint(ctx);
    EXPECT_EQ(result, Result::error_wrong_state);
}

TEST_F(RdmaTest, ConfigureEndpointSuccess) {
    // Mock the necessary setup for a successful configuration
    rdma.ep_ctx = reinterpret_cast<ep_ctx_t*>(0x1); // Assuming ep_ctx_t is the correct type
    size_t capacity = 10;
    size_t trx_sz = 128;
    rdma.init_queue_with_elements(capacity, trx_sz);

    Result result = rdma.configure_endpoint(ctx);
    EXPECT_EQ(result, Result::success);
}

TEST_F(RdmaTest, OnEstablishAlreadyInitialized) {
    rdma.init = true;
    Result result = rdma.on_establish(ctx);
    EXPECT_EQ(result, Result::error_already_initialized);
}

TEST_F(RdmaTest, OnEstablishSuccess) {
    // Mock the necessary setup for a successful establishment
    rdma.mDevHandle = reinterpret_cast<libfabric_ctx*>(0xdeadbeef); // Mock non-null mDevHandle
    size_t capacity = 10;
    size_t trx_sz = 128;
    rdma.init_queue_with_elements(capacity, trx_sz);

    Result result = rdma.on_establish(ctx);
    EXPECT_EQ(result, Result::success);
}

TEST_F(RdmaTest, OnShutdown) {
    Result result = rdma.on_shutdown(ctx);
    EXPECT_EQ(result, Result::success);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}