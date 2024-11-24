#include "conn_rdma_rx.h"
#include <stdexcept>

namespace mesh {

namespace connection {

RdmaRx::RdmaRx() : Rdma() {}

RdmaRx::~RdmaRx()
{
    // Any specific cleanup for Rx can go here
}

Result RdmaRx::configure(context::Context& ctx, const mcm_conn_param& request,
                         const std::string& dev_port, libfabric_ctx *& dev_handle)
{
    return Rdma::configure(ctx, request, dev_port, dev_handle, Kind::receiver, direction::RX);
}

Result RdmaRx::process_buffers(context::Context& ctx, void *buf, size_t sz)
{
    int err = libfabric_ep_ops.ep_recv_buf(ep_ctx, buf, sz, buf);
    if (err) {
        log::error("Failed to pass empty buffer to RDMA to receive into")
        ("buffer_address",buf)("error", fi_strerror(-err));
        return Result::error_general_failure;
    }

    return Result::success;
}

Result RdmaRx::handle_rdma_cq(context::Context& ctx, void *buf, size_t sz)
{
    while (!_ctx.cancelled()) {

        int err = libfabric_ep_ops.ep_cq_read(ep_ctx, (void **)&buf, RDMA_DEFAULT_TIMEOUT);
        if (err == -EAGAIN) {
            continue;
        } else if (err) {
            log::error("Completion queue read failed")("buffer_address", buf)
                      ("error",fi_strerror(-err));
            return Result::error_general_failure;
        }
        if (buf == nullptr) {
            log::error("Completion queue read returned a null buffer");
            return Result::error_general_failure;
        }
        break;
    }

    Result res = transmit(_ctx, buf, trx_sz);
    if (res != Result::success && !ctx.cancelled()) {
        log::error("Failed to transmit buffer")("buffer_address", buf)("size", trx_sz);
        return res;
    }
    return Result::success;
}

} // namespace connection

} // namespace mesh
