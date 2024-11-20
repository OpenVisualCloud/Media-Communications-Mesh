extern "C" {
#include "libfabric_ep.h"
#include "libfabric_mr.h"
}

#include <gtest/gtest.h>

#include "libfabric_mocks.h"

FAKE_VALUE_FUNC(int, av_insert, struct fid_av *, const void *, size_t, fi_addr_t *, uint64_t,
                void *);
FAKE_VALUE_FUNC(int, endpoint, struct fid_domain *, struct fi_info *, struct fid_ep **, void *);
FAKE_VALUE_FUNC(int, av_open, struct fid_domain *, struct fi_av_attr *, struct fid_av **, void *);
FAKE_VALUE_FUNC(ssize_t, send, struct fid_ep *, const void *, size_t, void *, fi_addr_t, void *);
FAKE_VALUE_FUNC(ssize_t, recv, struct fid_ep *, void *, size_t, void *, fi_addr_t, void *);
FAKE_VALUE_FUNC(int, rdma_cq_open_mock, ep_ctx_t *, size_t, enum cq_comp_method);
FAKE_VALUE_FUNC(int, rdma_read_cq_mock, ep_ctx_t *, struct fi_cq_err_entry *, int);
FAKE_VALUE_FUNC(int, rdma_cq_readerr_mock, struct fid_cq *);
FAKE_VALUE_FUNC(int, rdma_reg_mr_mock, libfabric_ctx *, struct fid_ep *, void *, size_t, uint64_t,
                uint64_t, enum fi_hmem_iface, uint64_t, struct fid_mr **, void **);
FAKE_VOID_FUNC(rdma_unreg_mr_mock, struct fid_mr *);
FAKE_VALUE_FUNC(uint64_t, rdma_info_to_mr_access_mock, struct fi_info *);

class LibfabricEpTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ops_msg = {.recv = recv, .send = send};
        ops_cq = {.read = cq_read};
        ops_av = {.insert = av_insert};
        ep_ops = {.close = custom_close, .bind = custom_bind, .control = control};
        av_and_cq_ops = {.close = custom_close};

        cq = {.ops = &ops_cq};
        av = {.ops = &ops_av};
        ep = {.msg = &ops_msg};
        cq_ctx = {.cq = &cq};
        ep.fid.ops = &ep_ops;
        av.fid.ops = &av_and_cq_ops;
        cq.fid.ops = &av_and_cq_ops;

        ops_domain = {.av_open = av_open, .endpoint = endpoint};
        domain = {.ops = &ops_domain};
        info = fi_allocinfo();
        info->ep_attr->type = FI_EP_RDM;
        rdma_ctx = {.domain = &domain, .info = info};

        ep_ctx = {.ep = &ep, .av = &av, .cq_ctx = cq_ctx, .rdma_ctx = &rdma_ctx};

        RESET_FAKE(control);
        RESET_FAKE(av_insert);
        RESET_FAKE(endpoint);
        RESET_FAKE(av_open);
        RESET_FAKE(fi_getinfo);
        RESET_FAKE(fi_freeinfo);
        RESET_FAKE(send);
        RESET_FAKE(recv);
        RESET_FAKE(cq_read);
        RESET_FAKE(rdma_cq_open_mock);
        RESET_FAKE(rdma_read_cq_mock);
        RESET_FAKE(rdma_cq_readerr_mock);
        RESET_FAKE(custom_bind);
        RESET_FAKE(custom_close);
        RESET_FAKE(rdma_reg_mr_mock);
        RESET_FAKE(rdma_unreg_mr_mock);
        RESET_FAKE(rdma_info_to_mr_access_mock);
    }

    static void SetUpTestSuite()
    {

        libfabric_cq_ops.rdma_read_cq = rdma_read_cq_mock;
        libfabric_cq_ops.rdma_cq_readerr = rdma_cq_readerr_mock;
        libfabric_cq_ops.rdma_cq_open = rdma_cq_open_mock;

        libfabric_mr_ops.rdma_reg_mr = rdma_reg_mr_mock;
        libfabric_mr_ops.rdma_info_to_mr_access = rdma_info_to_mr_access_mock;
        libfabric_mr_ops.rdma_unreg_mr = rdma_unreg_mr_mock;
    }

    static void TearDownTestSuite()
    {
        libfabric_cq_ops.rdma_read_cq = rdma_read_cq;
        libfabric_cq_ops.rdma_cq_readerr = rdma_cq_readerr;
        libfabric_cq_ops.rdma_cq_open = rdma_cq_open;

        libfabric_mr_ops.rdma_reg_mr = rdma_reg_mr;
        libfabric_mr_ops.rdma_info_to_mr_access = rdma_info_to_mr_access;
        libfabric_mr_ops.rdma_unreg_mr = rdma_unreg_mr;
    }

    void TearDown() override { fi_freeinfo(info); }
    libfabric_ctx rdma_ctx;

    struct fi_info *info;

    /* libfabric_ctx */
    struct fid_domain domain;

    fi_ops_domain ops_domain;

    ep_ctx_t ep_ctx;

    /* ep_ctx_t */
    cq_ctx_t cq_ctx;
    static struct fid_ep ep;
    static struct fid_av av;
    static struct fid_cq cq;

    static struct fi_ops ep_ops;
    static struct fi_ops av_and_cq_ops;
    static struct fi_ops_av ops_av;
    static struct fi_ops_msg ops_msg;
    static struct fi_ops_cq ops_cq;

    static int endpoint_custom_fake(struct fid_domain *domain, struct fi_info *info,
                                    struct fid_ep **ep, void *context)
    {
        *ep = &LibfabricEpTest::ep;
        return 0;
    }

    static int av_open_custom_fake(struct fid_domain *domain, struct fi_av_attr *attr,
                                   struct fid_av **av, void *context)
    {
        *av = &LibfabricEpTest::av;
        return 0;
    }

    static int rdma_cq_open_custom_fake(ep_ctx_t *ep_ctx, size_t cq_size,
                                        enum cq_comp_method comp_method)
    {
        ep_ctx->cq_ctx.cq = &LibfabricEpTest::cq;
        return 0;
    }
};

struct fid_ep LibfabricEpTest::ep;
struct fid_av LibfabricEpTest::av;
struct fid_cq LibfabricEpTest::cq;

struct fi_ops LibfabricEpTest::ep_ops;
struct fi_ops LibfabricEpTest::av_and_cq_ops;
struct fi_ops_av LibfabricEpTest::ops_av;
struct fi_ops_msg LibfabricEpTest::ops_msg;
struct fi_ops_cq LibfabricEpTest::ops_cq;

TEST_F(LibfabricEpTest, TestEpSendBufSuccess)
{
    send_fake.return_val = 0;

    int ret = libfabric_ep_ops.ep_send_buf(&ep_ctx, nullptr, 0);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(send_fake.call_count, 1);
    ASSERT_EQ(cq_read_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpSendBufFail)
{
    send_fake.return_val = -1;

    int ret = libfabric_ep_ops.ep_send_buf(&ep_ctx, nullptr, 0);

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(send_fake.call_count, 1);
    ASSERT_EQ(cq_read_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpSendBufRetrySuccess)
{
    ssize_t send_fake_return_vals[] = {-FI_EAGAIN, -FI_EAGAIN, 0};
    SET_RETURN_SEQ(send, send_fake_return_vals,
                   sizeof(send_fake_return_vals) / sizeof(send_fake_return_vals[0]));

    int ret = libfabric_ep_ops.ep_send_buf(&ep_ctx, nullptr, 0);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(send_fake.call_count, 3);
    ASSERT_EQ(cq_read_fake.call_count, 2);
}

TEST_F(LibfabricEpTest, TestEpSendBufRetryFail)
{
    ssize_t send_fake_return_vals[] = {-FI_EAGAIN, -FI_EAGAIN, -1};
    SET_RETURN_SEQ(send, send_fake_return_vals,
                   sizeof(send_fake_return_vals) / sizeof(send_fake_return_vals[0]));

    int ret = libfabric_ep_ops.ep_send_buf(&ep_ctx, nullptr, 0);

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(send_fake.call_count, 3);
    ASSERT_EQ(cq_read_fake.call_count, 2);
}

TEST_F(LibfabricEpTest, TestEpRecvBufFail)
{
    recv_fake.return_val = -1;

    int ret = libfabric_ep_ops.ep_recv_buf(&ep_ctx, nullptr, 0, nullptr);

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(recv_fake.call_count, 1);
}

TEST_F(LibfabricEpTest, TestEpRecvBufSuccess)
{
    recv_fake.return_val = 0;

    int ret = libfabric_ep_ops.ep_recv_buf(&ep_ctx, nullptr, 0, nullptr);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(recv_fake.call_count, 1);
}

TEST_F(LibfabricEpTest, TestEpRecvBufRetrySuccess)
{
    ssize_t recv_fake_return_vals[] = {-FI_EAGAIN, -FI_EAGAIN, 0};
    SET_RETURN_SEQ(recv, recv_fake_return_vals,
                   sizeof(recv_fake_return_vals) / sizeof(recv_fake_return_vals[0]));

    int ret = libfabric_ep_ops.ep_recv_buf(&ep_ctx, nullptr, 0, nullptr);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(recv_fake.call_count, 3);
    ASSERT_EQ(cq_read_fake.call_count, 2);
}

TEST_F(LibfabricEpTest, TestEpRecvBufRetryFail)
{
    ssize_t recv_fake_return_vals[] = {-FI_EAGAIN, -FI_EAGAIN, -1};
    SET_RETURN_SEQ(recv, recv_fake_return_vals,
                   sizeof(recv_fake_return_vals) / sizeof(recv_fake_return_vals[0]));

    int ret = libfabric_ep_ops.ep_recv_buf(&ep_ctx, nullptr, 0, nullptr);

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(recv_fake.call_count, 3);
    ASSERT_EQ(cq_read_fake.call_count, 2);
}

TEST_F(LibfabricEpTest, TestEpInitSuccessRX)
{
    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    av_open_fake.custom_fake = av_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx,
                    .remote_addr = {.ip = "127.0.0.1", .port = "12345"},
                    .local_addr = {.port = "12345"},
                    .dir = RX};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 1);
    ASSERT_EQ(control_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 2);
    ASSERT_EQ(av_insert_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpInitSuccessTX)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    av_open_fake.custom_fake = av_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = 1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx,
                    .remote_addr = {.ip = "127.0.0.1", .port = "12345"},
                    .local_addr = {.port = "12345"},
                    .dir = TX};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 2);
    ASSERT_EQ(control_fake.call_count, 1);
    ASSERT_EQ(av_insert_fake.call_count, 1);
    ASSERT_EQ(custom_close_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpInitSuccessDefault)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    av_open_fake.custom_fake = av_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = 1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 2);
    ASSERT_EQ(control_fake.call_count, 1);
    ASSERT_EQ(av_insert_fake.call_count, 1);
    ASSERT_EQ(custom_close_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpInitGetinfoFail)
{

    fi_getinfo_fake.return_val = -1;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    av_open_fake.custom_fake = av_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = 1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 0);
    ASSERT_EQ(av_open_fake.call_count, 0);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 0);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpInitEndpointFail)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.return_val = -1;
    av_open_fake.custom_fake = av_open_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = 1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 0);
    ASSERT_EQ(av_open_fake.call_count, 0);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpInitAv_openFail)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    av_open_fake.return_val = -1;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = 1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 2);
}

TEST_F(LibfabricEpTest, TestEpInitRdma_cq_openFail)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    rdma_cq_open_mock_fake.return_val = -1;
    av_open_fake.custom_fake = av_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = 1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 0);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 1);
}

TEST_F(LibfabricEpTest, TestEpInitEnableFail)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    av_open_fake.custom_fake = av_open_custom_fake;
    custom_bind_fake.return_val = 0;
    control_fake.return_val = -1;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = 1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 2);
    ASSERT_EQ(control_fake.call_count, 1);
    ASSERT_EQ(av_insert_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 3);
}

TEST_F(LibfabricEpTest, TestEpInitAv_insertFail)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    av_open_fake.custom_fake = av_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = -1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 2);
    ASSERT_EQ(control_fake.call_count, 1);
    ASSERT_EQ(av_insert_fake.call_count, 1);
    ASSERT_EQ(custom_close_fake.call_count, 3);
}

TEST_F(LibfabricEpTest, TestEpInitAv_insertReturnsNot1)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    av_open_fake.custom_fake = av_open_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = 0;
    av_insert_fake.return_val = 2;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -EINVAL);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 2);
    ASSERT_EQ(control_fake.call_count, 1);
    ASSERT_EQ(av_insert_fake.call_count, 1);
    ASSERT_EQ(custom_close_fake.call_count, 3);
}

TEST_F(LibfabricEpTest, TestEpInitBindFail)
{

    fi_getinfo_fake.custom_fake = fi_getinfo_custom_fake;
    endpoint_fake.custom_fake = endpoint_custom_fake;
    av_open_fake.custom_fake = av_open_custom_fake;
    rdma_cq_open_mock_fake.custom_fake = rdma_cq_open_custom_fake;
    control_fake.return_val = 0;
    custom_bind_fake.return_val = -1;
    av_insert_fake.return_val = 1;

    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    std::string captured_stderr;
    testing::internal::CaptureStderr();

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    captured_stderr = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(captured_stderr.empty());

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 1);
    ASSERT_EQ(endpoint_fake.call_count, 1);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 1);
    ASSERT_EQ(av_open_fake.call_count, 1);
    ASSERT_EQ(custom_bind_fake.call_count, 1);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 3);
}

TEST_F(LibfabricEpTest, TestEpInitNoCfg)
{
    ep_ctx_t *ep_ctx_ptr = nullptr;

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, nullptr);

    ASSERT_EQ(ret, -EINVAL);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 0);
    ASSERT_EQ(endpoint_fake.call_count, 0);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 0);
    ASSERT_EQ(av_open_fake.call_count, 0);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpInitEmptyCfg)
{
    ep_cfg_t cfg = {};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    ASSERT_EQ(ret, -EINVAL);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 0);
    ASSERT_EQ(endpoint_fake.call_count, 0);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 0);
    ASSERT_EQ(av_open_fake.call_count, 0);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpInitNoCtx)
{
    ep_cfg_t cfg = {.rdma_ctx = &rdma_ctx};
    int ret = libfabric_ep_ops.ep_init(nullptr, &cfg);

    ASSERT_EQ(ret, -EINVAL);
    ASSERT_EQ(fi_getinfo_fake.call_count, 0);
    ASSERT_EQ(endpoint_fake.call_count, 0);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 0);
    ASSERT_EQ(av_open_fake.call_count, 0);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpInitNoRdma_ctx)
{

    ep_cfg_t cfg = {.rdma_ctx = nullptr};
    ep_ctx_t *ep_ctx_ptr = nullptr;

    int ret = libfabric_ep_ops.ep_init(&ep_ctx_ptr, &cfg);

    ASSERT_EQ(ret, -EINVAL);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(fi_getinfo_fake.call_count, 0);
    ASSERT_EQ(endpoint_fake.call_count, 0);
    ASSERT_EQ(rdma_cq_open_mock_fake.call_count, 0);
    ASSERT_EQ(av_open_fake.call_count, 0);
    ASSERT_EQ(custom_bind_fake.call_count, 0);
    ASSERT_EQ(control_fake.call_count, 0);
    ASSERT_EQ(av_insert_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpCqReadSuccess)
{
    rdma_read_cq_mock_fake.return_val = 0;

    void *buf_ctx = nullptr;
    int ret = libfabric_ep_ops.ep_cq_read(&ep_ctx, &buf_ctx, 0);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(rdma_read_cq_mock_fake.call_count, 1);
}

TEST_F(LibfabricEpTest, TestEpCqReadFail)
{
    rdma_read_cq_mock_fake.return_val = -1;

    void *buf_ctx = nullptr;
    int ret = libfabric_ep_ops.ep_cq_read(&ep_ctx, &buf_ctx, 0);

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(rdma_read_cq_mock_fake.call_count, 1);
}

TEST_F(LibfabricEpTest, TestEpCqReadWithContext)
{
    struct fi_cq_err_entry entry = {.op_context = (void *)0xdeadbeef};
    rdma_read_cq_mock_fake.return_val = 0;
    rdma_read_cq_mock_fake.custom_fake = [](ep_ctx_t *, struct fi_cq_err_entry *entry, int) -> int {
        entry->op_context = (void *)0xdeadbeef;
        return 0;
    };

    void *buf_ctx = nullptr;
    int ret = libfabric_ep_ops.ep_cq_read(&ep_ctx, &buf_ctx, 0);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(buf_ctx, (void *)0xdeadbeef);
    ASSERT_EQ(rdma_read_cq_mock_fake.call_count, 1);
}

TEST_F(LibfabricEpTest, TestEpDestroySuccess)
{
    ep_ctx_t *ep_ctx_ptr = (ep_ctx_t *)malloc(sizeof(ep_ctx_t));
    if (ep_ctx_ptr == nullptr) {
        FAIL() << "Failed to allocate memory for ep_ctx_ptr";
    }
    memcpy(ep_ctx_ptr, &ep_ctx, sizeof(ep_ctx_t));

    int ret = libfabric_ep_ops.ep_destroy(&ep_ctx_ptr);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(rdma_unreg_mr_mock_fake.call_count, 1);
    ASSERT_EQ(custom_close_fake.call_count, 3); // ep, cq, av
}

TEST_F(LibfabricEpTest, TestEpDestroyNullCtx)
{
    ep_ctx_t *ep_ctx_ptr = nullptr;

    int ret = libfabric_ep_ops.ep_destroy(&ep_ctx_ptr);

    ASSERT_EQ(ret, -EINVAL);
    ASSERT_EQ(ep_ctx_ptr, nullptr);
    ASSERT_EQ(rdma_unreg_mr_mock_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpDestroyNullPtr)
{
    int ret = libfabric_ep_ops.ep_destroy(nullptr);

    ASSERT_EQ(ret, -EINVAL);
    ASSERT_EQ(rdma_unreg_mr_mock_fake.call_count, 0);
    ASSERT_EQ(custom_close_fake.call_count, 0);
}

TEST_F(LibfabricEpTest, TestEpRegMrSuccess)
{
    rdma_reg_mr_mock_fake.return_val = 0;

    int ret = libfabric_ep_ops.ep_reg_mr(&ep_ctx, nullptr, 0);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(rdma_reg_mr_mock_fake.call_count, 1);
}

TEST_F(LibfabricEpTest, TestEpRegMrFail)
{
    rdma_reg_mr_mock_fake.return_val = -1;

    int ret = libfabric_ep_ops.ep_reg_mr(&ep_ctx, nullptr, 0);

    ASSERT_EQ(ret, -1);
    ASSERT_EQ(rdma_reg_mr_mock_fake.call_count, 1);
}
