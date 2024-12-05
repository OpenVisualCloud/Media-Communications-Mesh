#ifndef RDMA_RX_H
#define RDMA_RX_H

#include "conn_rdma.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace mesh::connection
{

/**
 * RdmaRx
 *
 * Derived class for RDMA Receive operations.
 */
class RdmaRx : public Rdma
{
  public:
    RdmaRx();
    ~RdmaRx();

    // Configure the RDMA Receive session
    Result configure(context::Context& ctx, const mcm_conn_param& request,
                     const std::string& dev_port, libfabric_ctx *& dev_handle);

  protected:
    virtual Result start_threads(context::Context& ctx);

    // Receive data using RDMA
    void process_buffers_thread(context::Context& ctx);
    void rdma_cq_thread(context::Context& ctx);
};

} // namespace mesh::connection

#endif // RDMA_RX_H
