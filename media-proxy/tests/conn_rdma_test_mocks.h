#ifndef CONN_RDMA_TEST_MOCKS_H
#define CONN_RDMA_TEST_MOCKS_H

#include "libfabric_ep.h"
#include "libfabric_dev.h"
#include <iomanip>
#include <gmock/gmock.h>

// Define the mock classes
class MockLibfabricEpOps {
  public:
    MOCK_METHOD(int, ep_init, (ep_ctx_t **, ep_cfg_t *));
    MOCK_METHOD(int, ep_reg_mr, (ep_ctx_t *, void *, size_t));
    MOCK_METHOD(int, ep_destroy, (ep_ctx_t **));
    MOCK_METHOD(int, ep_cq_read, (ep_ctx_t *, void **, int));
    MOCK_METHOD(int, ep_send_buf, (ep_ctx_t *, void *, size_t));
    MOCK_METHOD(int, ep_recv_buf, (ep_ctx_t *, void *, size_t, void *));
};

class MockLibfabricDevOps {
  public:
    MOCK_METHOD(int, rdma_init, (libfabric_ctx **));
};

//Helper functions
std::string to_hex(const void* data, size_t size);

// Declare global mock objects
extern MockLibfabricEpOps *mock_ep_ops;
extern MockLibfabricDevOps *mock_dev_ops;

// Declare mock function pointers
int MockEpInit(ep_ctx_t **ep_ctx, ep_cfg_t *cfg);
int MockEpRegMr(ep_ctx_t *ep_ctx, void *data_buf, size_t data_buf_size);
int MockEpDestroy(ep_ctx_t **ep_ctx);
int MockEpCqRead(ep_ctx_t *ep_ctx, void **buf_ctx, int timeout);
int MockEpSendBuf(ep_ctx_t *ep_ctx, void *buffer, size_t size);
int MockEpRecvBuf(ep_ctx_t *ep_ctx, void *buffer, size_t size, void *buf_ctx);
int MockRdmaInit(libfabric_ctx **rdma_ctx);

// Declare mock setup functions
void SetUpMockEpOps();
void SetUpMockDevOps();

#endif // CONN_RDMA_TEST_MOCKS_H