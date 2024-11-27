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
template <typename FRAME, typename HANDLE, typename OPS> class ST2110Rx : public ST2110 {
  public:
    ST2110Rx() { _kind = Kind::receiver; }

    ~ST2110Rx() {
        shutdown(_ctx);
        if (ops.name)
            free((void *)ops.name);
    }

  protected:
    HANDLE mtl_session = nullptr;
    OPS ops = {0};
    size_t transfer_size = 0;
    std::jthread frame_thread_handle;
    context::Context _ctx = context::WithCancel(context::Background());

    virtual FRAME *get_frame(HANDLE) = 0;
    virtual int put_frame(HANDLE, FRAME *) = 0;
    virtual HANDLE create_session(mtl_handle, OPS *) = 0;
    virtual int close_session(HANDLE) = 0;

    int configure_common(context::Context& ctx, const std::string& dev_port,
                         const MeshConfig_ST2110& cfg_st2110) {
        int session_id = 0;
        mtl_device = get_mtl_device(dev_port, MTL_LOG_LEVEL_CRIT, cfg_st2110.local_ip_addr, session_id);
        if (!mtl_device) {
            log::error("Failed to get MTL device");
            return -1;
        }

        inet_pton(AF_INET, cfg_st2110.remote_ip_addr, ops.port.ip_addr[MTL_PORT_P]);
        inet_pton(AF_INET, cfg_st2110.local_ip_addr, ops.port.mcast_sip_addr[MTL_PORT_P]);
        ops.port.udp_port[MTL_PORT_P] = cfg_st2110.local_port;

        strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
        ops.port.num_port = 1;

        char session_name[NAME_MAX] = "";
        snprintf(session_name, NAME_MAX, "mcm_mtl_rx_%d", session_id);
        if (ops.name)
            free((void *)ops.name);
        ops.name = strdup(session_name);
        ops.framebuff_cnt = 4;

        ops.priv = this; // app handle register to lib
        ops.notify_frame_available = frame_available_cb;

        log::info("ST2110Rx: configure")
            ("port", ops.port.port[MTL_PORT_P])
            ("ip_addr", std::to_string(ops.port.ip_addr[MTL_PORT_P][0]) + " " +
                        std::to_string(ops.port.ip_addr[MTL_PORT_P][1]) + " " +
                        std::to_string(ops.port.ip_addr[MTL_PORT_P][2]) + " " +
                        std::to_string(ops.port.ip_addr[MTL_PORT_P][3]))
            ("mcast_sip_addr", std::to_string(ops.port.mcast_sip_addr[MTL_PORT_P][0]) + " " +
                               std::to_string(ops.port.mcast_sip_addr[MTL_PORT_P][1]) + " " +
                               std::to_string(ops.port.mcast_sip_addr[MTL_PORT_P][2]) + " " +
                               std::to_string(ops.port.mcast_sip_addr[MTL_PORT_P][3]))
            ("num_port", ops.port.num_port)
            ("udp_port", ops.port.udp_port[MTL_PORT_P])
            ("name", ops.name)
            ("framebuff_cnt", ops.framebuff_cnt);

        return 0;
    }

    Result on_establish(context::Context& ctx) override {
        _ctx = context::WithCancel(ctx);
        stop = false;

        mtl_session = create_session(mtl_device, &ops);
        if (!mtl_session) {
            log::error("Failed to create session");
            set_state(ctx, State::closed);
            return set_result(Result::error_general_failure);
        }

        /* Start MTL session thread. */
        try {
            frame_thread_handle = std::jthread(&ST2110Rx::frame_thread, this);
        } catch (const std::system_error& e) {
            log::error("Failed to create thread");
            set_state(ctx, State::closed);
            return set_result(Result::error_out_of_memory);
        }

        set_state(ctx, State::active);
        return set_result(Result::success);
    }

    Result on_shutdown(context::Context& ctx) override {
        _ctx.cancel();
        stop = true;
        stop.notify_all();

        frame_thread_handle.join();

        if (mtl_session) {
            close_session(mtl_session);
            mtl_session = nullptr;
        }
        set_state(ctx, State::closed);
        return set_result(Result::success);
    };

    virtual void on_delete(context::Context& ctx) override {}

  private:
    void frame_thread() {
        while (!_ctx.cancelled()) {
            // Get full buffer from MTL
            FRAME *frame_ptr = get_frame(mtl_session);
            if (!frame_ptr) { /* no frame */
                stop.wait(false);
                stop = false;
                continue;
            }
            // Forward buffer to emulated receiver
            transmit(_ctx, get_frame_data_ptr(frame_ptr), transfer_size);
            // Return used buffer to MTL
            put_frame(mtl_session, frame_ptr);
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
