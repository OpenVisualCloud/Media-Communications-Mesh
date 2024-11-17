#ifndef RDMA_RX_H
#define RDMA_RX_H

#include "conn_rdma.h"

namespace mesh {

namespace connection {

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
    Result configure(context::Context &ctx, const mcm_conn_param &request, 
                     const std::string &dev_port, libfabric_ctx *&dev_handle);

protected:
    // Override buffer handling for Rx
    virtual Result handle_buffers(context::Context& ctx, void* buffer, size_t size) override;
    virtual Result process_completion_event(void* buf_ctx) override;
    void frame_thread(context::Context &ctx) override;

    // Receive data using RDMA
    Result receive_data(context::Context& ctx, void* buffer, size_t buffer_size);
};

} // namespace connection

} // namespace mesh

#endif // RDMA_RX_H
