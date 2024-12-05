/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ST2110TX_H
#define ST2110TX_H

#include "st2110.h"

namespace mesh::connection {

/**
 * ST2110Tx
 *
 * Base abstract class of ST2110Tx. ST2110_20Tx/ST2110_22Tx/ST2110_30Tx
 * inherit this class.
 */
template <typename FRAME, typename HANDLE, typename OPS>
class ST2110Tx : public ST2110<FRAME, HANDLE, OPS> {
  public:
    ST2110Tx() { this->_kind = Kind::transmitter; };

  protected:
    int configure_common(context::Context& ctx, const std::string& dev_port,
                         const MeshConfig_ST2110& cfg_st2110) override {
        int ret = ST2110<FRAME, HANDLE, OPS>::configure_common(ctx, dev_port, cfg_st2110);
        if (ret) {
            return ret;
        }

        inet_pton(AF_INET, cfg_st2110.remote_ip_addr, this->ops.port.dip_addr[MTL_PORT_P]);
        this->ops.port.udp_port[MTL_PORT_P] = cfg_st2110.remote_port;
        this->ops.port.udp_src_port[MTL_PORT_P] = cfg_st2110.local_port;

        log::info("ST2110Tx: configure")
            ("dip_addr", std::to_string(this->ops.port.dip_addr[MTL_PORT_P][0]) + " " +
                         std::to_string(this->ops.port.dip_addr[MTL_PORT_P][1]) + " " +
                         std::to_string(this->ops.port.dip_addr[MTL_PORT_P][2]) + " " +
                         std::to_string(this->ops.port.dip_addr[MTL_PORT_P][3]))
            ("udp_port", this->ops.port.udp_port[MTL_PORT_P])
            ("udp_src_port", this->ops.port.udp_src_port[MTL_PORT_P]);
        return 0;
    }

    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent) override {
        int copy_size = this->transfer_size > sz ? sz : this->transfer_size;

        FRAME *frame;
        for (;;) {
            if (ctx.cancelled() || this->_ctx.cancelled())
                return this->set_result(Result::error_context_cancelled);
            
            // Get empty buffer from MTL
            frame = this->get_frame(this->mtl_session);
            if (frame)
                break;
            
            this->wait_frame_available();
        }

        // Copy data from emulated transmitter to MTL empty buffer
        mtl_memcpy(get_frame_data_ptr(frame), ptr, copy_size);
        // Return full buffer to MTL
        this->put_frame(this->mtl_session, frame);

        sent = this->transfer_size;
        return this->set_result(Result::success);
    };
};

class ST2110_20Tx : public ST2110Tx<st_frame, st20p_tx_handle, st20p_tx_ops> {
  public:
    Result configure(context::Context& ctx, const std::string& dev_port,
                     const MeshConfig_ST2110& cfg_st2110, const MeshConfig_Video& cfg_video);

  protected:
    st_frame *get_frame(st20p_tx_handle h) override;
    int put_frame(st20p_tx_handle h, st_frame *f) override;
    st20p_tx_handle create_session(mtl_handle h, st20p_tx_ops *o) override;
    int close_session(st20p_tx_handle h) override;
};

class ST2110_22Tx : public ST2110Tx<st_frame, st22p_tx_handle, st22p_tx_ops> {
  public:
    Result configure(context::Context& ctx, const std::string& dev_port,
                     const MeshConfig_ST2110& cfg_st2110, const MeshConfig_Video& cfg_video);

  protected:
    st_frame *get_frame(st22p_tx_handle h) override;
    int put_frame(st22p_tx_handle h, st_frame *f) override;
    st22p_tx_handle create_session(mtl_handle h, st22p_tx_ops *o) override;
    int close_session(st22p_tx_handle h) override;
};

class ST2110_30Tx : public ST2110Tx<st30_frame, st30p_tx_handle, st30p_tx_ops> {
  public:
    Result configure(context::Context& ctx, const std::string& dev_port,
                     const MeshConfig_ST2110& cfg_st2110, const MeshConfig_Audio& cfg_audio);

  protected:
    st30_frame *get_frame(st30p_tx_handle h) override;
    int put_frame(st30p_tx_handle h, st30_frame *f) override;
    st30p_tx_handle create_session(mtl_handle h, st30p_tx_ops *o) override;
    int close_session(st30p_tx_handle h) override;
};

} // namespace mesh::connection

#endif // ST2110TX_H
