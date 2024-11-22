#include "conn_rdma_tx.h"
#include <stdexcept>

namespace mesh
{

namespace connection
{

RdmaTx::RdmaTx() : Rdma() {}

RdmaTx::~RdmaTx()
{
    // Any specific cleanup for Tx can go here
}

Result RdmaTx::configure(context::Context& ctx, const mcm_conn_param& request,
                         const std::string& dev_port,
                         libfabric_ctx *& dev_handle)
{
    return Rdma::configure(ctx, request, dev_port, dev_handle,
                           Kind::transmitter, direction::TX);
}

Result RdmaTx::handle_buffers(context::Context& ctx, void *buf, size_t size)
{
    int err = libfabric_ep_ops.ep_cq_read(ep_ctx, &buf, RDMA_DEFAULT_TIMEOUT);
    if (err == -EAGAIN) {
        return Result::success;
    } else if (err) {
        log::error("Completion queue read failed")("error", fi_strerror(-err));
        return Result::error_general_failure;
    }
    return Result::success;
}

Result RdmaTx::on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                          uint32_t& sent)
{
    uint32_t tmp_sent = std::min(static_cast<uint32_t>(trx_sz), sz);
    if (tmp_sent != trx_sz) {
        log::warn("Sent size differs from transfer size")(
                  "requested_size", tmp_sent)("trx_sz", trx_sz);
    }

    if (bufs.empty() || !bufs[0]) {
        log::error("No pre-allocated buffer available for RDMA transmission");
        sent = 0;
        return set_result(Result::error_general_failure);
    }

    void *reg_buf = bufs[0];

    std::memcpy(reg_buf, ptr, tmp_sent);

    std::string buffer_contents(static_cast<char *>(reg_buf),
                                tmp_sent);

    int err = libfabric_ep_ops.ep_send_buf(ep_ctx, reg_buf, tmp_sent);
    if (err) {
        log::error("Failed to send buffer through RDMA")
                  ("error",fi_strerror(-err));
        sent = 0;
        return set_result(Result::error_general_failure);
    }

    sent = tmp_sent;
    return set_result(Result::success);
}

} // namespace connection

} // namespace mesh
