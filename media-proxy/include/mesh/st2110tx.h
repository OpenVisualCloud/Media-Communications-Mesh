#ifndef ST2110TX_H
#define ST2110TX_H

#include "st2110.h"
#include <algorithm>

namespace mesh::connection {

/**
 * ST2110Tx
 *
 * Base abstract class of ST2110Tx. ST2110_20Tx/ST2110_22Tx/ST2110_30Tx
 * inherit this class.
 */
template <typename FRAME, typename HANDLE, typename OPS> class ST2110Tx : public ST2110 {
  public:
    ST2110Tx() : mtl_session(nullptr), ops{0}, transfer_size(0) { _kind = Kind::transmitter; };
    ~ST2110Tx() {
        shutdown(_ctx);
        if (ops.name)
            free((void *)ops.name);
    };

  protected:
    HANDLE mtl_session;
    OPS ops;
    uint32_t transfer_size;
    context::Context _ctx = context::WithCancel(context::Background());

    virtual FRAME *get_frame(HANDLE) = 0;
    virtual int put_frame(HANDLE, FRAME *) = 0;
    virtual HANDLE create_session(mtl_handle, OPS *) = 0;
    virtual int close_session(HANDLE) = 0;

    int configure_common(context::Context& ctx, const std::string& dev_port,
                         const MeshConfig_ST2110& cfg_st2110) {
        int session_id = 0;
        mtl_device = get_mtl_device(dev_port, MTL_LOG_LEVEL_CRIT, cfg_st2110.local_ip_addr, session_id);
        if (mtl_device == nullptr) {
            log::error("Failed to get MTL device");
            return -1;
        }

        inet_pton(AF_INET, cfg_st2110.remote_ip_addr, ops.port.dip_addr[MTL_PORT_P]);
        ops.port.udp_port[MTL_PORT_P] = cfg_st2110.remote_port;
        strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
        ops.port.udp_src_port[MTL_PORT_P] = cfg_st2110.local_port;
        ops.port.num_port = 1;

        char session_name[NAME_MAX] = "";
        snprintf(session_name, NAME_MAX, "mcm_mtl_tx_%d", session_id);
        if (ops.name)
            free((void *)ops.name);
        ops.name = strdup(session_name);
        ops.framebuff_cnt = 4;

        ops.priv = this; // app handle register to lib
        ops.notify_frame_available = frame_available_cb;

        log::info("ST2110Tx: configure")("port", ops.port.port[MTL_PORT_P])(
            "dip_addr", std::to_string(ops.port.dip_addr[MTL_PORT_P][0]) + " " +
                            std::to_string(ops.port.dip_addr[MTL_PORT_P][1]) + " " +
                            std::to_string(ops.port.dip_addr[MTL_PORT_P][2]) + " " +
                            std::to_string(ops.port.dip_addr[MTL_PORT_P][3]))(
            "num_port", ops.port.num_port)("udp_port", ops.port.udp_port[MTL_PORT_P])(
            "udp_src_port", ops.port.udp_src_port[MTL_PORT_P])("name", ops.name)("framebuff_cnt",
                                                                                 ops.framebuff_cnt);

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

        set_state(ctx, State::active);
        return set_result(Result::success);
    };

    Result on_shutdown(context::Context& ctx) override {
        _ctx.cancel();

        if (mtl_session) {
            close_session(mtl_session);
            mtl_session = nullptr;
        }
        set_state(ctx, State::closed);
        return set_result(Result::success);
    };

    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent) override {
        int to_be_sent = std::min(transfer_size, sz);
        // TODO: add error/warning if sent is different than _transfer_size

        FRAME *frame = NULL;
        do {
            // Get empty buffer from MTL
            frame = get_frame(mtl_session);
            if (!frame) {
                std::unique_lock lk(mx);
                cv.wait(lk, _ctx.stop_token(), [this] { return stop.load(); });
                stop = false;
                if (_ctx.cancelled()) {
                    return set_result(Result::error_shutdown);
                }
            }
        } while (!frame);

        if (frame) {
            // Copy data from emulated transmitter to MTL empty buffer
            mtl_memcpy(get_frame_data_ptr(frame), ptr, to_be_sent);
            // Return full buffer to MTL
            put_frame(mtl_session, frame);
        } else {
            return set_result(Result::error_shutdown);
        }

        sent = to_be_sent;
        return set_result(Result::success);
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
