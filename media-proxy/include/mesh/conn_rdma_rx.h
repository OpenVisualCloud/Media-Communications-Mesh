#ifndef RDMA_RX_H
#define RDMA_RX_H

#include "conn_rdma.h"

namespace mesh
{

namespace connection
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
    // Override buffer handling for Rx
    virtual Result handle_rdma_cq(context::Context& ctx, void *buffer, size_t size) override;
    virtual Result process_buffers(context::Context& ctx, void *buf, size_t sz) override;
    virtual Result start_threads(context::Context& ctx) override;
    // void frame_thread(context::Context &ctx) override;

    // Receive data using RDMA
    void process_buffers_thread(context::Context& ctx) override;
    void rdma_cq_thread(context::Context& ctx) override;
    std::mutex cq_mutex;
    std::condition_variable_any cq_cv;
    bool event_ready = false;
    void notify_cq_event();
};

} // namespace connection

} // namespace mesh

#endif // RDMA_RX_H
