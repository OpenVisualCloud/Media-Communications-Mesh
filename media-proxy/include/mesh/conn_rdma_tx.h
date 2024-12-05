#ifndef RDMA_TX_H
#define RDMA_TX_H

#include "conn_rdma.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace mesh {

namespace connection {

/**
 * RdmaTx
 *
 * Derived class for RDMA Transmit operations.
 */
class RdmaTx : public Rdma {
  public:
    RdmaTx();
    ~RdmaTx();

    // Configure the RDMA Transmit session
    Result configure(context::Context& ctx, const mcm_conn_param& request,
                     const std::string& dev_port, libfabric_ctx *& dev_handle);

    // Transmit data using RDMA when received from connection class
    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent);

  protected:
    virtual Result start_threads(context::Context& ctx);
    void rdma_cq_thread(context::Context& ctx);

    size_t total_sent = 0;
};

} // namespace connection

} // namespace mesh

#endif // RDMA_TX_H
