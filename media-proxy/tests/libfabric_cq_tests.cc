extern "C" {
#include "libfabric_cq.h"
#include "libfabric_ep.h"
}

#include <gtest/gtest.h>
#include "libfabric_mocks.h"

// Mock functions
FAKE_VALUE_FUNC(ssize_t, cq_readerr, struct fid_cq*, struct fi_cq_err_entry*, uint64_t);
FAKE_VALUE_FUNC(ssize_t, cq_sread, struct fid_cq*, void*, size_t, const void*, int);
FAKE_VALUE_FUNC(int, cq_open, struct fid_domain*, struct fi_cq_attr*, struct fid_cq**, void*);
FAKE_VALUE_FUNC(int, trywait, struct fid_fabric*, struct fid**, int);
FAKE_VALUE_FUNC(int, poll, struct pollfd*, nfds_t, int);
FAKE_VALUE_FUNC(const char *, cq_strerror, struct fid_cq*, int, const void*, char*, size_t);

class LibfabricCqTest : public ::testing::Test {
protected:
    void SetUp() override {
        ops_cq = {.read = cq_read, .readerr = cq_readerr, .sread = cq_sread, .strerror = cq_strerror};
        ops = {.control = &control};
        cq = {.fid = {.ops = &ops}, .ops = &ops_cq};
        cq_ctx = {.cq = &cq};
        ops_domain = {.cq_open = cq_open};
        domain = {.ops = &ops_domain};
        ops_fabric = {.trywait = trywait};
        fabric = {.ops = &ops_fabric};
        rdma_ctx = {.fabric = &fabric, .domain = &domain};

        ep_ctx = {.cq_ctx = cq_ctx, .rdma_ctx = &rdma_ctx};

        RESET_FAKE(cq_read);
        RESET_FAKE(cq_readerr);
        RESET_FAKE(cq_sread);
        RESET_FAKE(cq_open);
        RESET_FAKE(control);
        RESET_FAKE(trywait);
        RESET_FAKE(poll);
        RESET_FAKE(cq_strerror);
    }

    libfabric_ctx rdma_ctx;
    ep_ctx_t ep_ctx;
    cq_ctx_t cq_ctx;
    struct fi_ops ops;
    struct fid_cq cq;
    struct fi_ops_cq ops_cq;
    fid_domain domain;
    fid_fabric fabric;
    struct fi_ops_domain ops_domain;
    struct fi_ops_fabric ops_fabric;

    static struct fi_cq_err_entry cq_err;

    static ssize_t cq_readerr_custom_fake (struct fid_cq *cq, struct fi_cq_err_entry *buf, uint64_t flags) {
        cq_err = {.err = FI_EAVAIL}; /* this returns positive error code */
        *buf = cq_err;
        return 1;
    };

};

struct fi_cq_err_entry LibfabricCqTest::cq_err;


TEST_F(LibfabricCqTest, TestRdmaReadCqSuccessFd) {
    cq_open_fake.return_val = 0;
    control_fake.return_val = 0;
    poll_fake.return_val = POLLIN;

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_WAIT_FD);
    ASSERT_EQ(ret, 0);

    cq_read_fake.return_val = 1;

    struct fi_cq_err_entry entry;
    ret = rdma_read_cq(&ep_ctx, &entry, 0);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cq_read_fake.call_count, 1);
}

TEST_F(LibfabricCqTest, TestRdmaReadCqFdTimeout) {
    cq_open_fake.return_val = 0;
    control_fake.return_val = 0;
    poll_fake.return_val = 0;

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_WAIT_FD);
    ASSERT_EQ(ret, 0);

    cq_read_fake.return_val = 1;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    struct fi_cq_err_entry entry;
    ret = rdma_read_cq(&ep_ctx, &entry, 0);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());
    

    ASSERT_EQ(ret, -EAGAIN);
    ASSERT_EQ(cq_read_fake.call_count, 0);
}

TEST_F(LibfabricCqTest, TestRdmaReadCqSuccessSpin) {
    cq_open_fake.return_val = 0;

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_SPIN);
    ASSERT_EQ(ret, 0);

    cq_read_fake.return_val = 1;

    struct fi_cq_err_entry entry;
    ret = rdma_read_cq(&ep_ctx, &entry, 0);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cq_read_fake.call_count, 1);
}

TEST_F(LibfabricCqTest, TestRdmaReadCqSuccessSread) {
    cq_open_fake.return_val = 0;

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_SREAD);
    ASSERT_EQ(ret, 0);

    cq_read_fake.return_val = 1;

    struct fi_cq_err_entry entry;
    ret = rdma_read_cq(&ep_ctx, &entry, 0);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cq_sread_fake.call_count, 1);
}

TEST_F(LibfabricCqTest, TestRdmaReadCqFail) {
    cq_open_fake.return_val = 0;

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_SPIN);
    ASSERT_EQ(ret, 0);

    cq_read_fake.return_val = -FI_EAVAIL;
    cq_readerr_fake.return_val = 0;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    struct fi_cq_err_entry entry;
    ret = rdma_read_cq(&ep_ctx, &entry, 0);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cq_read_fake.call_count, 1);
    ASSERT_EQ(cq_readerr_fake.call_count, 1);
}

TEST_F(LibfabricCqTest, TestRdmaReadCqEagain) {
    cq_open_fake.return_val = 0;

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_SPIN);
    ASSERT_EQ(ret, 0);

    cq_read_fake.return_val = -FI_EAGAIN;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    struct fi_cq_err_entry entry;
    ret = rdma_read_cq(&ep_ctx, &entry, 0);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -ENODATA); /* timeout = 0, cq_read returned -FI_EAGAIN, so no data is available*/
    ASSERT_EQ(cq_read_fake.call_count, 1);
}

TEST_F(LibfabricCqTest, TestRdmaCqReaderrSuccess) {
    cq_readerr_fake.return_val = 1;
    cq_readerr_fake.custom_fake = cq_readerr_custom_fake;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = rdma_cq_readerr(&cq);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -FI_EAVAIL);
    ASSERT_EQ(cq_readerr_fake.call_count, 1);
}

TEST_F(LibfabricCqTest, TestRdmaCqReaderrFail) {
    cq_readerr_fake.return_val = -1;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = rdma_cq_readerr(&cq);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(cq_readerr_fake.call_count, 1);
}

TEST_F(LibfabricCqTest, TestRdmaCqOpenSuccess) {
    cq_open_fake.return_val = 0;
    control_fake.return_val = 0;

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_WAIT_FD);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cq_open_fake.call_count, 1);
    ASSERT_EQ(control_fake.call_count, 1);
}

TEST_F(LibfabricCqTest, TestRdmaCqOpenFail) {
    cq_open_fake.return_val = -1;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_WAIT_FD);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(cq_open_fake.call_count, 1);
    ASSERT_EQ(control_fake.call_count, 0);
}

TEST_F(LibfabricCqTest, TestRdmaCqOpenEnableFail) {
    cq_open_fake.return_val = 0;
    control_fake.return_val = -1;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = rdma_cq_open(&ep_ctx, 10, RDMA_COMP_WAIT_FD);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(cq_open_fake.call_count, 1);
    ASSERT_EQ(control_fake.call_count, 1);
}
