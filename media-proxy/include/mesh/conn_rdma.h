#ifndef CONN_RDMA_H
#define CONN_RDMA_H

#include "logger.h"
#include "mesh/conn.h"
#include "concurrency.h"   // Include concurrency support
#include "libfabric_ep.h"  // For ep_init, ep_destroy, etc.
#include "libfabric_mr.h"  // For ep_reg_mr
#include "libfabric_cq.h"  // For ep_cq_read, ep_cq_open
#include "libfabric_dev.h" // For libfabric_ctx
#include "mcm_dp.h"        // For mcm_conn_param
#include <vector>          // For buffer storage
#include <mutex>           // For thread safety
#include <cstring>         // For std::memcpy/memset
#include <cstddef>         // For size_t
#include <thread>
#include <atomic>

#include <queue>

#include <thread>
#include <atomic>
#include <condition_variable>
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
                             const std::string& dev_port, libfabric_ctx*& dev_handle, Kind kind,
                             direction dir);

    // Overrides from Connection
    virtual Result on_establish(context::Context& ctx) override;
    virtual void on_delete(context::Context& ctx) override;
    virtual Result on_shutdown(context::Context& ctx) override;

    // RDMA-specific methods

    // Configure RDMA endpoint
    Result configure_endpoint(context::Context& ctx);

    // Handle buffers (must be implemented in derived classes)
    virtual Result handle_rdma_cq(context::Context& ctx, void *buffer, size_t size) = 0;
    virtual Result process_buffers(context::Context& ctx, void *buf, size_t sz) = 0;

    // Cleanup RDMA resources
    Result cleanup_resources(context::Context& ctx);

    // Error handler for logging and recovery
    void handle_error(context::Context& ctx, const char *step);

    // RDMA-specific members
    libfabric_ctx *mDevHandle; // RDMA device handle
    ep_ctx_t *ep_ctx;          // Endpoint context
    ep_cfg_t ep_cfg;           // Endpoint configuration
    size_t trx_sz;             // Data transfer size
    bool init;                 // Initialization flag

    // RDMA thread logic
    virtual void process_buffers_thread(context::Context& ctx) = 0;
    virtual void rdma_cq_thread(context::Context& ctx) = 0;
    virtual Result start_threads(context::Context& ctx) = 0;

    Result init_queue_with_elements(size_t capacity, size_t trx_sz);
    Result add_to_queue(void* element);
    Result consume_from_queue(context::Context& ctx, void** element);
    void cleanup_queue();


    // Queue for managing buffers
    std::queue<void*> buffer_queue;
    std::mutex queue_mutex;
    std::condition_variable_any queue_cv;

    std::jthread handle_process_buffers_thread; // thread handle
    std::jthread handle_rdma_cq_thread; // thread handle
    context::Context process_buffers_thread_ctx;
    context::Context rdma_cq_thread_ctx;

    std::mutex cq_mutex;
    std::condition_variable_any cq_cv;
    bool event_ready = false;

    void notify_cq_event();


  private:
    void shutdown_rdma(context::Context& ctx);
};

} // namespace connection

} // namespace mesh

#endif // CONN_RDMA_H
