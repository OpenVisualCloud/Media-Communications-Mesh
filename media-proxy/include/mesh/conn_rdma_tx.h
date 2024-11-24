#ifndef RDMA_TX_H
#define RDMA_TX_H

#include "conn_rdma.h"

namespace mesh
{

namespace connection
{

/**
 * RdmaTx
 *
 * Derived class for RDMA Transmit operations.
 */
class RdmaTx : public Rdma
{
  public:
    RdmaTx();
    ~RdmaTx();

    // Configure the RDMA Transmit session
    Result configure(context::Context& ctx, const mcm_conn_param& request,
                     const std::string& dev_port, libfabric_ctx *& dev_handle);

  protected:
    // Override buffer handling for Tx
    virtual Result handle_rdma_cq(context::Context& ctx, void *buffer, size_t size) override;
    virtual Result process_buffers(context::Context& ctx, void *buf, size_t sz)
      {
        //Shouldn't be invoked in this ctx
        return Result::error_general_failure;
      };
    virtual Result start_threads(context::Context& ctx) override;
    // Transmit data using RDMA when received from connection class
    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                      uint32_t& sent);

    // Send data using RDMA
    void process_buffers_thread(context::Context& ctx) {
        // Shouldn't be invoked in this ctx
    };
    void rdma_cq_thread(context::Context& ctx) override;
    std::mutex cq_mutex;
    std::condition_variable cq_cv;
    bool event_ready = false;
    void notify_cq_event();
};

} // namespace connection

} // namespace mesh

#endif // RDMA_TX_H
