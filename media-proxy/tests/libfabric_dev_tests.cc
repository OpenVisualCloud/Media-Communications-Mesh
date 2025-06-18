// Define FFF globals exactly once.
#define DEFINE_FFF_GLOBALS
#include <fff.h>

#include "libfabric_dev_tests.h"
#include <rdma/rdma_cma.h>
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Additional fake declarations not present in libfabric_mocks.h.
FAKE_VALUE_FUNC(int, fi_fabric, struct fi_fabric_attr *, struct fid_fabric **, void *);
FAKE_VALUE_FUNC(int, domain, struct fid_fabric *, struct fi_info *, struct fid_domain **, void *);

// rdma_getaddrinfo and rdma_freeaddrinfo declarations matching the system header.
FAKE_VALUE_FUNC(int, rdma_getaddrinfo, const char*, const char*, const struct rdma_addrinfo*, struct rdma_addrinfo**);
FAKE_VOID_FUNC(rdma_freeaddrinfo, struct rdma_addrinfo*);


// ---------------------------------------------------------------------------
// Custom fake for rdma_getaddrinfo: allocate a minimal dummy structure.
static int rdma_getaddrinfo_custom_fake(const char *ip, const char *port,
                                          const struct rdma_addrinfo *hints,
                                          struct rdma_addrinfo **res) {
    *res = (struct rdma_addrinfo *)malloc(sizeof(struct rdma_addrinfo));
    // Set minimal fields for both RX and TX.
    (*res)->ai_src_len = sizeof(struct sockaddr_in);
    (*res)->ai_src_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr_in));
    memset((*res)->ai_src_addr, 0, sizeof(struct sockaddr_in));
    (*res)->ai_dst_len = sizeof(struct sockaddr_in);
    (*res)->ai_dst_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr_in));
    memset((*res)->ai_dst_addr, 0, sizeof(struct sockaddr_in));
    return 0;
}

// Custom fake for rdma_freeaddrinfo: free the allocated structure.
static void rdma_freeaddrinfo_custom_fake(struct rdma_addrinfo *res) {
    if (res) {
        if (res->ai_src_addr)
            free(res->ai_src_addr);
        if (res->ai_dst_addr)
            free(res->ai_dst_addr);
        free(res);
    }
}

// ---------------------------------------------------------------------------
// Global objects for our additional mocks.
static struct fi_ops_fabric ops_fabric;
static struct fid_domain dom;
static fid_fabric fabric;
static struct fi_ops ops;

// Custom fake implementations for fi_fabric and domain.
static int fi_fabric_custom_fake(struct fi_fabric_attr *attr, struct fid_fabric **fabric, void *context) {
    *fabric = &::fabric;
    return 0;
}

static int domain_custom_fake(struct fid_fabric *fabric, struct fi_info *info,
                                struct fid_domain **dom, void *context) {
    *dom = &::dom;
    return 0;
}

// ---------------------------------------------------------------------------
// Test fixture implementation.
void LibfabricDevTest::SetUp() {
    // Allocate a dummy libfabric context.
    ctx = (libfabric_ctx*)malloc(sizeof(libfabric_ctx));
    memset(ctx, 0, sizeof(libfabric_ctx));
    // Set receiver parameters.
    ctx->kind = KIND_RECEIVER;
    ctx->local_ip = strdup("127.0.0.1");
    ctx->local_port = strdup("12345");

    // Initialize additional mock objects.
    ops_fabric.domain = domain;
    ops.close = custom_close;  // custom_close is defined in libfabric_mocks.cc.
    fabric.fid.ops = &ops;
    fabric.ops = &ops_fabric;
    dom.fid.ops = &ops;

    // Reset all fakes.
    RESET_FAKE(fi_getinfo);
    RESET_FAKE(fi_freeinfo);
    RESET_FAKE(fi_fabric);
    RESET_FAKE(domain);
    RESET_FAKE(rdma_getaddrinfo);
    RESET_FAKE(rdma_freeaddrinfo);

    // Set custom fake functions.
    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake; // Defined in libfabric_mocks.cc.
    fi_fabric_fake.custom_fake = fi_fabric_custom_fake;
    domain_fake.return_val = 0;
    rdma_getaddrinfo_fake.custom_fake = rdma_getaddrinfo_custom_fake;
    rdma_freeaddrinfo_fake.custom_fake = rdma_freeaddrinfo_custom_fake;
}

void LibfabricDevTest::TearDown() {
    if (ctx)
        libfabric_dev_ops.rdma_deinit(&ctx);
}

// ---------------------------------------------------------------------------
// Tests

TEST_F(LibfabricDevTest, TestRdmaInitSuccess) {
    int ret = libfabric_dev_ops.rdma_init(&ctx);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(ctx, nullptr);
    // Expect one call each for fi_getinfo, fi_fabric, and domain, and one free of hints.
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(fi_fabric_fake.call_count, 1);
    ASSERT_EQ(domain_fake.call_count, 1);
    ASSERT_EQ(fi_freeinfo_fake.call_count, 1);
    libfabric_dev_ops.rdma_deinit(&ctx);
}

TEST_F(LibfabricDevTest, TestRdmaInitFailGetinfo) {
    fi_getinfo_fake.custom_fake = NULL;
    fi_getinfo_fake.return_val = -1;
    
    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_dev_ops.rdma_init(&ctx);
    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());
    
    ASSERT_EQ(ret, -1);
    ASSERT_FALSE(ctx->is_initialized);
    
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(fi_fabric_fake.call_count, 0);
    ASSERT_EQ(domain_fake.call_count, 0);
    ASSERT_EQ(fi_freeinfo_fake.call_count, 1);
}

TEST_F(LibfabricDevTest, TestRdmaInitFailFabric) {
    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    fi_fabric_fake.custom_fake = NULL;
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
    // Expect two freeinfo calls: one for hints and one for partially allocated info.
    ASSERT_EQ(fi_freeinfo_fake.call_count, 2);
}

TEST_F(LibfabricDevTest, TestRdmaInitFailDomain) {
    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    fi_fabric_fake.custom_fake = fi_fabric_custom_fake;
    domain_fake.custom_fake = NULL;
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
    // Expect two freeinfo calls: one for hints and one for partially allocated info.
    ASSERT_EQ(fi_freeinfo_fake.call_count, 2);
}

TEST_F(LibfabricDevTest, TestRdmaDeinitSuccess) {
    int ret = libfabric_dev_ops.rdma_init(&ctx);
    ASSERT_NE(ctx, nullptr);
    ret = libfabric_dev_ops.rdma_deinit(&ctx);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(ctx, nullptr);
    // Expect a total of two freeinfo calls.
    ASSERT_EQ(fi_freeinfo_fake.call_count, 2);
}

TEST_F(LibfabricDevTest, TestRdmaInitNullContext) {
    int ret = libfabric_dev_ops.rdma_init(nullptr);
    ASSERT_EQ(ret, -EINVAL);
}
