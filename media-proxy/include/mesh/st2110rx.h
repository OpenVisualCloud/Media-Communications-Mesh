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
    ST2110Rx() : mtl_session(nullptr), ops{0}, transfer_size(0) { _kind = Kind::receiver; }

    ~ST2110Rx() { shutdown(_ctx); }

  protected:
    HANDLE mtl_session;
    OPS ops;
    size_t transfer_size;
    std::jthread frame_thread_handle;
    context::Context _ctx;

    virtual FRAME *get_frame(HANDLE) = 0;
    virtual int put_frame(HANDLE, FRAME *) = 0;
    virtual HANDLE create_session(mtl_handle, OPS *) = 0;
    virtual int close_session(HANDLE) = 0;

    Result on_establish(context::Context &ctx) override
    {
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
        } catch (const std::system_error &e) {
            log::error("Failed to create thread");
            set_state(ctx, State::closed);
            return set_result(Result::error_out_of_memory);
        }

        set_state(ctx, State::active);
        return set_result(Result::success);
    }

    Result on_shutdown(context::Context &ctx) override
    {
        _ctx.cancel();

        frame_thread_handle.join();

        if (mtl_session) {
            close_session(mtl_session);
            mtl_session = nullptr;
        }
        set_state(ctx, State::closed);
        return set_result(Result::success);
    };

    virtual void on_delete(context::Context &ctx) override {}

  private:
    void frame_thread()
    {
        while (!_ctx.cancelled()) {
            // Get full buffer from MTL
            FRAME *frame_ptr = get_frame(mtl_session);
            if (!frame_ptr) { /* no frame */
                std::unique_lock lk(mx);
                cv.wait(lk, _ctx.stop_token(), [this] { return stop.load(); });
                stop = false;
                if (_ctx.cancelled()) {
                    return;
                }
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
    ST2110_20Rx() {};
    ~ST2110_20Rx();

    Result configure(context::Context &ctx, const std::string &dev_port,
                     const MeshConfig_ST2110 &cfg_st2110, const MeshConfig_Video &cfg_video);

  protected:
    st_frame *get_frame(st20p_rx_handle h) override;
    int put_frame(st20p_rx_handle h, st_frame *f) override;
    st20p_rx_handle create_session(mtl_handle h, st20p_rx_ops *o) override;
    int close_session(st20p_rx_handle h) override;
};

class ST2110_22Rx : public ST2110Rx<st_frame, st22p_rx_handle, st22p_rx_ops> {
  public:
    ST2110_22Rx() {};
    ~ST2110_22Rx();

    Result configure(context::Context &ctx, const std::string &dev_port,
                     const MeshConfig_ST2110 &cfg_st2110, const MeshConfig_Video &cfg_video);

  protected:
    st_frame *get_frame(st22p_rx_handle h) override;
    int put_frame(st22p_rx_handle h, st_frame *f) override;
    st22p_rx_handle create_session(mtl_handle h, st22p_rx_ops *o) override;
    int close_session(st22p_rx_handle h) override;
};

class ST2110_30Rx : public ST2110Rx<st30_frame, st30p_rx_handle, st30p_rx_ops> {
  public:
    ST2110_30Rx() {};
    ~ST2110_30Rx();

    Result configure(context::Context &ctx, const std::string &dev_port,
                     const MeshConfig_ST2110 &cfg_st2110, const MeshConfig_Audio &cfg_audio);

  protected:
    st30_frame *get_frame(st30p_rx_handle h) override;
    int put_frame(st30p_rx_handle h, st30_frame *f) override;
    st30p_rx_handle create_session(mtl_handle h, st30p_rx_ops *o) override;
    int close_session(st30p_rx_handle h) override;
};

} // namespace mesh::connection

#endif // ST2110RX_H
