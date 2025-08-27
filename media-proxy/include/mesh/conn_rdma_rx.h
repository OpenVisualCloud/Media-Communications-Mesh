#ifndef RDMA_RX_H
#define RDMA_RX_H

#include "conn_rdma.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace mesh::connection {

/**
 * RdmaRx
 *
 * Derived class for RDMA Receive operations.
 */
class RdmaRx : public Rdma {
public:
    RdmaRx();
    ~RdmaRx();

    // Configure the RDMA Receive session
    Result configure(context::Context& ctx, const mcm_conn_param& request,
                     libfabric_ctx *& dev_handle);

protected:
    virtual Result start_threads(context::Context& ctx);

    // Receive data using RDMA
    void process_buffers_thread(context::Context& ctx);
    void rdma_cq_thread(context::Context& ctx);
    std::atomic<uint32_t> next_rx_idx;
    static constexpr size_t REORDER_WINDOW = 256; // > max expected out-of-order
    std::array<void *, REORDER_WINDOW> reorder_ring{{nullptr}};
    uint64_t reorder_head = UINT64_MAX;
};

} // namespace mesh::connection

#endif // RDMA_RX_H
