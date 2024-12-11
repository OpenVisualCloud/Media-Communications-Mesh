/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ST2110RX_H
#define ST2110RX_H

#include "st2110.h"

namespace mesh::connection {

/**
 * ST2110Rx
 *
 * Base abstract class of ST2110Rx. ST2110_20Rx/ST2110_22Rx/ST2110_30Rx
 * inherit this class.
 */
template <typename FRAME, typename HANDLE, typename OPS>
class ST2110Rx : public ST2110<FRAME, HANDLE, OPS> {
  public:
    ST2110Rx() { this->_kind = Kind::receiver; }

  protected:
    std::jthread frame_thread_handle;

    int configure_common(context::Context& ctx, const std::string& dev_port,
                         const MeshConfig_ST2110& cfg_st2110) override {
        int ret = ST2110<FRAME, HANDLE, OPS>::configure_common(ctx, dev_port, cfg_st2110);
        if (ret) {
            return ret;
        }

        inet_pton(AF_INET, cfg_st2110.remote_ip_addr, this->ops.port.ip_addr[MTL_PORT_P]);
        inet_pton(AF_INET, cfg_st2110.local_ip_addr, this->ops.port.mcast_sip_addr[MTL_PORT_P]);
        this->ops.port.udp_port[MTL_PORT_P] = cfg_st2110.local_port;

        log::info("ST2110Rx: configure")
            ("ip_addr", std::to_string(this->ops.port.ip_addr[MTL_PORT_P][0]) + " " +
                        std::to_string(this->ops.port.ip_addr[MTL_PORT_P][1]) + " " +
                        std::to_string(this->ops.port.ip_addr[MTL_PORT_P][2]) + " " +
                        std::to_string(this->ops.port.ip_addr[MTL_PORT_P][3]))
            ("mcast_sip_addr", std::to_string(this->ops.port.mcast_sip_addr[MTL_PORT_P][0]) + " " +
                               std::to_string(this->ops.port.mcast_sip_addr[MTL_PORT_P][1]) + " " +
                               std::to_string(this->ops.port.mcast_sip_addr[MTL_PORT_P][2]) + " " +
                               std::to_string(this->ops.port.mcast_sip_addr[MTL_PORT_P][3]))
            ("udp_port", this->ops.port.udp_port[MTL_PORT_P]);
        return 0;
    }

    Result on_establish(context::Context& ctx) override {
        Result res = ST2110<FRAME, HANDLE, OPS>::on_establish(ctx);
        if (res != Result::success) {
            return res;
        }

        /* Start MTL session thread. */
        try {
            frame_thread_handle = std::jthread(&ST2110Rx::frame_thread, this);
        } catch (const std::system_error& e) {
            log::error("Failed to create thread");
            this->set_state(ctx, State::closed);
            return this->set_result(Result::error_out_of_memory);
        }

        this->set_state(ctx, State::active);
        return this->set_result(Result::success);
    }

    Result on_shutdown(context::Context& ctx) override {
        Result res = ST2110<FRAME, HANDLE, OPS>::on_shutdown(ctx);
        if (res != Result::success) {
            return res;
        }

        frame_thread_handle.join();

        this->set_state(ctx, State::closed);
        return this->set_result(Result::success);
    };

  private:
    void frame_thread() {
        while (!this->_ctx.cancelled()) {
            // Get full buffer from MTL
            FRAME *frame_ptr = this->get_frame(this->mtl_session);
            if (frame_ptr) {
                // Forward buffer to emulated receiver
                this->transmit(this->_ctx, get_frame_data_ptr(frame_ptr), this->transfer_size);
                // Return used buffer to MTL
                this->put_frame(this->mtl_session, frame_ptr);
            } else {
                this->wait_frame_available();
            }
        }
    }
};

class ST2110_20Rx : public ST2110Rx<st_frame, st20p_rx_handle, st20p_rx_ops> {
  public:
    Result configure(context::Context& ctx, const std::string& dev_port,
                     const MeshConfig_ST2110& cfg_st2110, const MeshConfig_Video& cfg_video);

  protected:
    st_frame *get_frame(st20p_rx_handle h) override;
    int put_frame(st20p_rx_handle h, st_frame *f) override;
    st20p_rx_handle create_session(mtl_handle h, st20p_rx_ops *o) override;
    int close_session(st20p_rx_handle h) override;
};

class ST2110_22Rx : public ST2110Rx<st_frame, st22p_rx_handle, st22p_rx_ops> {
  public:
    Result configure(context::Context& ctx, const std::string& dev_port,
                     const MeshConfig_ST2110& cfg_st2110, const MeshConfig_Video& cfg_video);

  protected:
    st_frame *get_frame(st22p_rx_handle h) override;
    int put_frame(st22p_rx_handle h, st_frame *f) override;
    st22p_rx_handle create_session(mtl_handle h, st22p_rx_ops *o) override;
    int close_session(st22p_rx_handle h) override;
};

class ST2110_30Rx : public ST2110Rx<st30_frame, st30p_rx_handle, st30p_rx_ops> {
  public:
    Result configure(context::Context& ctx, const std::string& dev_port,
                     const MeshConfig_ST2110& cfg_st2110, const MeshConfig_Audio& cfg_audio);

  protected:
    st30_frame *get_frame(st30p_rx_handle h) override;
    int put_frame(st30p_rx_handle h, st30_frame *f) override;
    st30p_rx_handle create_session(mtl_handle h, st30p_rx_ops *o) override;
    int close_session(st30p_rx_handle h) override;
};

} // namespace mesh::connection

#endif // ST2110RX_H
