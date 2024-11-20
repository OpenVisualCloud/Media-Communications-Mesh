extern "C" {
#include "libfabric_mr.h"
}

#include <gtest/gtest.h>
#include "libfabric_mocks.h"

// Mock functions
FAKE_VALUE_FUNC(int, mr_regattr ,struct fid *, const struct fi_mr_attr *, uint64_t, struct fid_mr **);

class LibfabricMrTest : public ::testing::Test {
protected:
    void SetUp() override {
        ops_mr = {.regattr = mr_regattr};
        domain = {.mr = &ops_mr};
        domain_attr.mr_mode = 0;
        info.domain_attr = &domain_attr;
        info.caps = 0;
        info.mode = 0;
        rdma_ctx = {.domain = &domain, .info = &info};
        ops = {.close = custom_close, .bind = custom_bind, .control = control};
        f_mr = { .fid = {.ops = &ops}};

        RESET_FAKE(mr_regattr);
        RESET_FAKE(custom_bind);
        RESET_FAKE(custom_close);
        RESET_FAKE(control);
    }
    static struct fid_mr f_mr;
    libfabric_ctx rdma_ctx;
    struct fi_info info;
    struct fi_domain_attr domain_attr;
    struct fi_ops ops;

    struct fid_domain domain;
    struct fi_ops_mr ops_mr;

    static int mr_regattr_custom_fake(struct fid *fid, const struct fi_mr_attr *attr,
                                      uint64_t flags, struct fid_mr **mr)
    {
        *mr = &LibfabricMrTest::f_mr;
        return 0;
    }
};

struct fid_mr LibfabricMrTest::f_mr;

TEST_F(LibfabricMrTest, TestRdmaRegMrSuccess) {
    struct fid_mr *mr = nullptr;
    void* desc = nullptr;
    mr_regattr_fake.custom_fake = mr_regattr_custom_fake;
    custom_bind_fake.return_val = 0;
    control_fake.return_val = 0;

    int ret = libfabric_mr_ops.rdma_reg_mr(&rdma_ctx, nullptr, nullptr, 0, 0, 0, FI_HMEM_SYSTEM, 0,
                                           &mr, &desc);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(mr, nullptr);
    ASSERT_EQ(mr_regattr_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    
}

TEST_F(LibfabricMrTest, TestRdmaRegMrFailRegattr) {
    struct fid_mr* mr = nullptr;
    void* desc = nullptr;
    mr_regattr_fake.return_val = -1;

    int ret = libfabric_mr_ops.rdma_reg_mr(&rdma_ctx, nullptr, nullptr, 0, 0, 0, FI_HMEM_SYSTEM, 0,
                                           &mr, &desc);

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(mr, nullptr);
    ASSERT_EQ(mr_regattr_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
}

TEST_F(LibfabricMrTest, TestRdmaRegMrFailBind) {
    struct fid_mr* mr = nullptr;
    void* desc = nullptr;
    mr_regattr_fake.custom_fake = mr_regattr_custom_fake;
    custom_bind_fake.return_val = -1;

    rdma_ctx.info->domain_attr->mr_mode = FI_MR_ENDPOINT;

    int ret = libfabric_mr_ops.rdma_reg_mr(&rdma_ctx, nullptr, nullptr, 0, 0, 0, FI_HMEM_SYSTEM, 0,
                                           &mr, &desc);
    ASSERT_EQ(ret, -1);
    ASSERT_EQ(mr_regattr_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 1);
    ASSERT_EQ(control_fake.call_count, 0);
}

TEST_F(LibfabricMrTest, TestRdmaRegMrFailEnable) {
    struct fid_mr* mr = &f_mr;
    void* desc = nullptr;
    mr_regattr_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    control_fake.return_val = -1;

    rdma_ctx.info->domain_attr->mr_mode = FI_MR_ENDPOINT;

    int ret = libfabric_mr_ops.rdma_reg_mr(&rdma_ctx, nullptr, nullptr, 0, 0, 0, FI_HMEM_SYSTEM, 0,
                                           &mr, &desc);

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(mr_regattr_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 1);
    ASSERT_EQ(control_fake.call_count, 1);
}

TEST_F(LibfabricMrTest, TestRdmaUnregMr) {
    struct fid_mr* mr = (struct fid_mr*)malloc(sizeof(struct fid_mr));
    ASSERT_NE(mr, nullptr);
    memcpy(mr, &f_mr, sizeof(struct fid_mr));

    libfabric_mr_ops.rdma_unreg_mr(mr);

    free(mr);
    ASSERT_EQ(custom_close_fake.call_count, 1);
}

TEST_F(LibfabricMrTest, TestRdmaInfoToMrAccess) {
    info.caps = FI_MSG | FI_TAGGED | FI_SEND | FI_RECV;
    info.mode = FI_LOCAL_MR;
    uint64_t access = libfabric_mr_ops.rdma_info_to_mr_access(&info);

    ASSERT_EQ(access, FI_SEND | FI_RECV);
}

TEST_F(LibfabricMrTest, TestRdmaInfoToMrAccessRma) {
    info.caps = FI_RMA | FI_ATOMIC | FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE;
    info.mode = FI_LOCAL_MR;
    uint64_t access = libfabric_mr_ops.rdma_info_to_mr_access(&info);

    ASSERT_EQ(access, FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE);
}

TEST_F(LibfabricMrTest, TestRdmaInfoToMrAccessRemote) {
    info.caps = FI_RMA | FI_ATOMIC;
    info.mode = 0;
    uint64_t access = libfabric_mr_ops.rdma_info_to_mr_access(&info);

    ASSERT_EQ(access, FI_REMOTE_READ | FI_REMOTE_WRITE);
}

TEST_F(LibfabricMrTest, TestRdmaInfoToMrAccessRemoteNoCaps) {
    info.caps = 0;
    info.mode = 0;
    uint64_t access = libfabric_mr_ops.rdma_info_to_mr_access(&info);

    ASSERT_EQ(access, 0);
}

TEST_F(LibfabricMrTest, TestRdmaInfoToMrAccessFiContext) {
    info.caps = FI_MSG;
    info.mode = FI_CONTEXT;
    uint64_t access = libfabric_mr_ops.rdma_info_to_mr_access(&info);

    ASSERT_EQ(access, 0);
}

