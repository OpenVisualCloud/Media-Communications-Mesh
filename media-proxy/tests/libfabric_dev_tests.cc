extern "C" {
#include "libfabric_dev.h"
}

#include <gtest/gtest.h>
#include "libfabric_mocks.h"

// Mock functions
FAKE_VALUE_FUNC(int, fi_fabric, struct fi_fabric_attr *, struct fid_fabric **, void *);
FAKE_VALUE_FUNC(int, domain, struct fid_fabric *, struct fi_info *, struct fid_domain **, void *);

class LibfabricDevTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ops_fabric = {.domain = domain};
        ops = {.close = custom_close};
        fabric = {.fid = {.ops = &ops}, .ops = &ops_fabric};
        dom = {.fid = {.ops = &ops}};

        RESET_FAKE(fi_getinfo);
        RESET_FAKE(fi_freeinfo);
        RESET_FAKE(fi_fabric);
        RESET_FAKE(domain);
    }

    void TearDown() override {}

    static struct fi_ops_fabric ops_fabric;
    static struct fid_domain dom;
    static fid_fabric fabric;
    static struct fi_ops ops;

    static int fi_fabric_custom_fake(struct fi_fabric_attr *attr, struct fid_fabric **fabric,
                                     void *context)
    {
        *fabric = &LibfabricDevTest::fabric;
        return 0;
    }
    static int domain_custom_fake(struct fid_fabric *fabric, struct fi_info *info,
                                  struct fid_domain **dom, void *context)
    {
        *dom = &LibfabricDevTest::dom;
        return 0;
    }
};

struct fid_fabric LibfabricDevTest::fabric;
struct fi_ops_fabric LibfabricDevTest::ops_fabric;
struct fi_ops LibfabricDevTest::ops;
struct fid_domain LibfabricDevTest::dom;

TEST_F(LibfabricDevTest, TestRdmaInitSuccess)
{
    libfabric_ctx *ctx = nullptr;
    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    fi_fabric_fake.custom_fake = fi_fabric_custom_fake;
    domain_fake.return_val = 0;

    int ret = libfabric_dev_ops.rdma_init(&ctx);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(fi_fabric_fake.call_count, 1);
    ASSERT_EQ(domain_fake.call_count, 1);
    ASSERT_EQ(fi_freeinfo_fake.call_count, 1);

    libfabric_dev_ops.rdma_deinit(&ctx);
}

TEST_F(LibfabricDevTest, TestRdmaInitFailGetinfo)
{
    libfabric_ctx *ctx = nullptr;
    fi_getinfo_fake.return_val = -1;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_dev_ops.rdma_init(&ctx);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ctx, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(fi_fabric_fake.call_count, 0);
    ASSERT_EQ(domain_fake.call_count, 0);
    ASSERT_EQ(fi_freeinfo_fake.call_count, 1);
}

TEST_F(LibfabricDevTest, TestRdmaInitFailFabric)
{
    libfabric_ctx *ctx = nullptr;
    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    fi_fabric_fake.return_val = -1;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_dev_ops.rdma_init(&ctx);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ctx, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(fi_fabric_fake.call_count, 1);
    ASSERT_EQ(domain_fake.call_count, 0);
    ASSERT_EQ(fi_freeinfo_fake.call_count, 2);
}

TEST_F(LibfabricDevTest, TestRdmaInitFailDomain)
{
    libfabric_ctx *ctx = nullptr;
    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    fi_fabric_fake.custom_fake = fi_fabric_custom_fake;
    domain_fake.return_val = -1;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_dev_ops.rdma_init(&ctx);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ctx, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(fi_fabric_fake.call_count, 1);
    ASSERT_EQ(domain_fake.call_count, 1);
    ASSERT_EQ(fi_freeinfo_fake.call_count, 2);
}

TEST_F(LibfabricDevTest, TestRdmaDeinitSuccess)
{
    libfabric_ctx *ctx = nullptr;
    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    fi_fabric_fake.custom_fake = fi_fabric_custom_fake;
    domain_fake.return_val = 0;

    libfabric_dev_ops.rdma_init(&ctx);
    ASSERT_NE(ctx, nullptr);

    int ret = libfabric_dev_ops.rdma_deinit(&ctx);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(ctx, nullptr);
    ASSERT_EQ(fi_freeinfo_fake.call_count, 2);
}
