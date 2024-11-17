#ifndef CONN_RDMA_H
#define CONN_RDMA_H

#include "mesh/conn.h"
#include "concurrency.h"
#include "libfabric_ep.h"  // For ep_init, ep_destroy, etc.
#include "libfabric_mr.h"  // For ep_reg_mr
#include "libfabric_cq.h"  // For ep_cq_read, ep_cq_open
#include "libfabric_dev.h" // For libfabric_ctx
#include "mcm_dp.h"        // For mcm_conn_param
#include <thread>
#include <atomic>
#include <vector>  // For buffer storage
#include <cstring> // For std::memcpy

#ifndef RDMA_DEFAULT_TIMEOUT
#define RDMA_DEFAULT_TIMEOUT 1 // Set to 1 millisecond or any appropriate value
#endif

namespace mesh
{

namespace connection
{

/**
 * Rdma
 *
 * Base class for RDMA connections, derived from the `Connection` class.
 * Provides common RDMA-related functionality and acts as a foundation
 * for specialized RDMA Tx and Rx classes.
 */
class Rdma : public Connection
{
  public:
    Rdma();
    virtual ~Rdma();

  protected:
    // Configure the RDMA session
    virtual Result configure(context::Context &ctx, const mcm_conn_param &request,
                             const std::string &dev_port, libfabric_ctx *&dev_handle, Kind kind,
                             direction dir);

    // Overrides from Connection
    virtual Result on_establish(context::Context &ctx) override;
    virtual Result on_shutdown(context::Context &ctx) override;

    // RDMA-specific methods
    Result configure_endpoint(context::Context &ctx); // Configure RDMA endpoint
    virtual Result handle_buffers(context::Context &ctx, void *buffer,
                                  size_t size) = 0;  // Handle buffers
    Result cleanup_resources(context::Context &ctx); // Cleanup RDMA resources
    void check_context_cancelled(context::Context &ctx);

    // Allocate shared buffer
    Result allocate_buffer(size_t buffer_count, size_t buffer_size);

    // Start and stop the RDMA frame thread
    Result start_thread(context::Context &parent_ctx);
    void stop_thread_func();

    // RDMA-specific members
    libfabric_ctx *mDevHandle; // RDMA device handle
    ep_ctx_t *ep_ctx;        // Endpoint context
    ep_cfg_t ep_cfg;          // Endpoint configuration
    size_t transfer_size;      // Data transfer size
    bool initialized;          // Initialization flag

    // Helper methods (must be implemented by derived classes)
    virtual Result process_completion_event(void *buf_ctx) = 0;

  
    virtual void frame_thread(context::Context &ctx); // RDMA frame thread logic

    // Member variables
    std::vector<void *> buffers;      // Shared buffers
    size_t buffer_count;              // Number of allocated buffers
    std::thread *frame_thread_handle; // Pointer to the frame thread
    std::atomic<bool> stop_thread;    // Flag to signal thread stop
    context::Context thread_ctx;      // Context for the frame thread
    private:
};

} // namespace connection

} // namespace mesh

#endif // CONN_RDMA_H
