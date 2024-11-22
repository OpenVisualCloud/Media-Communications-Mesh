#ifndef CONN_RDMA_H
#define CONN_RDMA_H

#include "mesh/conn.h"
#include "concurrency.h" // Include concurrency support
#include "logger.h"
#include "libfabric_ep.h"  // For ep_init, ep_destroy, etc.
#include "libfabric_mr.h"  // For ep_reg_mr
#include "libfabric_cq.h"  // For ep_cq_read, ep_cq_open
#include "libfabric_dev.h" // For libfabric_ctx
#include "mcm_dp.h"        // For mcm_conn_param
#include <cstring>         // For std::memset
#include <thread>
#include <atomic>
#include <vector>  // For buffer storage
#include <cstring> // For std::memcpy

#ifndef RDMA_DEFAULT_TIMEOUT
#define RDMA_DEFAULT_TIMEOUT 1 // Set to 1 millisecond || appropriate value
#endif

namespace mesh {

namespace connection {

/**
 * Rdma
 *
 * Base class for RDMA connections, derived from the `Connection` class.
 * Provides common RDMA-related functionality and acts as a foundation
 * for specialized RDMA Tx and Rx classes.
 */
class Rdma : public Connection {
  public:
    Rdma();
    virtual ~Rdma();

  protected:
    // Configure the RDMA session
    virtual Result configure(context::Context& ctx, const mcm_conn_param& request,
                             const std::string& dev_port, libfabric_ctx *& dev_handle, Kind kind,
                             direction dir);

    // Overrides from Connection
    virtual Result on_establish(context::Context& ctx) override;
    virtual void on_delete(context::Context& ctx);
    virtual Result on_shutdown(context::Context& ctx) override;

    // RDMA-specific methods

    // Configure RDMA endpoint
    Result configure_endpoint(context::Context& ctx);

    // Handle buffers
    virtual Result handle_buffers(context::Context& ctx, void *buffer, size_t size) = 0;
    
    // Cleanup RDMA resources
    Result cleanup_resources(context::Context& ctx);

    // Allocate shared buffer
    Result allocate_buffer(size_t buffer_count, size_t buffer_size);

    // Handle error
    void handle_error(context::Context& ctx, const char *step);

    // RDMA-specific members
    libfabric_ctx *mDevHandle; // RDMA device handle
    ep_ctx_t *ep_ctx;          // Endpoint context
    ep_cfg_t ep_cfg;           // Endpoint configuration
    size_t trx_sz;             // Data transfer size
    bool init;                 // Initialization flag
    virtual Result process_buffers(context::Context& ctx, void *buf, size_t sz)
    {
        return Result::success;
    }

    virtual void frame_thread(); // RDMA frame thread logic

    // Member variables
    std::vector<void *> bufs;         // Shared buffers
    size_t buf_cnt;                   // Number of allocated buffers
    std::jthread frame_thread_handle; // frame thread handle
    context::Context _ctx;            // Inner class context
  private:
    void shutdown_rdma(context::Context& ctx);
};

} // namespace connection

} // namespace mesh

#endif // CONN_RDMA_H
