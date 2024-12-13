#include "conn_rdma_test_mocks.h"

MockLibfabricEpOps *mock_ep_ops = nullptr;
MockLibfabricDevOps *mock_dev_ops = nullptr;

// Utility function to convert data to a hex string
std::string to_hex(const void* data, size_t size) {
    std::ostringstream oss;
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

// Mock setup functions
void SetUpMockEpOps() {
    libfabric_ep_ops.ep_init = [](ep_ctx_t **ep_ctx, ep_cfg_t *cfg) -> int {
        return mock_ep_ops->ep_init(ep_ctx, cfg);
    };
    libfabric_ep_ops.ep_reg_mr = [](ep_ctx_t *ep_ctx, void *buf, size_t size) -> int {
        return mock_ep_ops->ep_reg_mr(ep_ctx, buf, size);
    };
    libfabric_ep_ops.ep_destroy = [](ep_ctx_t **ep_ctx) -> int {
        return mock_ep_ops->ep_destroy(ep_ctx);
    };
    libfabric_ep_ops.ep_cq_read = [](ep_ctx_t *ep_ctx, void **buf_ctx, int timeout) -> int {
        return mock_ep_ops->ep_cq_read(ep_ctx, buf_ctx, timeout);
    };
    libfabric_ep_ops.ep_send_buf = [](ep_ctx_t *ep_ctx, void *buf, size_t size) -> int {
        return mock_ep_ops->ep_send_buf(ep_ctx, buf, size);
    };
    libfabric_ep_ops.ep_recv_buf = [](ep_ctx_t *ep_ctx, void *buf, size_t size, void *buf_ctx) -> int {
        return mock_ep_ops->ep_recv_buf(ep_ctx, buf, size, buf_ctx);
    };
}

void SetUpMockDevOps() {
    libfabric_dev_ops.rdma_init = [](libfabric_ctx **rdma_ctx) -> int {
        return mock_dev_ops->rdma_init(rdma_ctx);
    };
    libfabric_dev_ops.rdma_deinit = [](libfabric_ctx **ctx) -> int { // Add this lambda
        return mock_dev_ops->rdma_deinit(ctx);
    };
}

// Define mock function pointers
int MockEpInit(ep_ctx_t **ep_ctx, ep_cfg_t *cfg) { return mock_ep_ops->ep_init(ep_ctx, cfg); }

int MockEpRegMr(ep_ctx_t *ep_ctx, void *data_buf, size_t data_buf_size)
{
    return mock_ep_ops->ep_reg_mr(ep_ctx, data_buf, data_buf_size);
}

int MockEpDestroy(ep_ctx_t **ep_ctx) { return mock_ep_ops->ep_destroy(ep_ctx); }

int MockEpCqRead(ep_ctx_t *ep_ctx, void **buf_ctx, int timeout)
{
    return mock_ep_ops->ep_cq_read(ep_ctx, buf_ctx, timeout);
}

int MockEpSendBuf(ep_ctx_t *ep_ctx, void *buffer, size_t size)
{
    return mock_ep_ops->ep_send_buf(ep_ctx, buffer, size);
}

int MockEpRecvBuf(ep_ctx_t *ep_ctx, void *buffer, size_t size, void *buf_ctx)
{
    return mock_ep_ops->ep_recv_buf(ep_ctx, buffer, size, buf_ctx);
}

int MockRdmaInit(libfabric_ctx **rdma_ctx) { return mock_dev_ops->rdma_init(rdma_ctx); }

