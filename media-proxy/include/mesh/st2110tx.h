#ifndef ST2110TX_H
#define ST2110TX_H

#include "st2110.h"
#include <algorithm>

namespace mesh::connection {

/**
 * ST2110
 *
 * Base abstract class of ST2110. ST2110_20Tx/ST2110_22Tx/ST2110_30Tx
 * inherit this class.
 */
template <typename FRAME, typename HANDLE, typename OPS> class ST2110Tx : public ST2110 {
  public:
    ST2110Tx()
    {
        _kind = Kind::transmitter;

        _handle = nullptr;
        _ops = {0};
        _transfer_size = 0;
    };
    ~ST2110Tx() { shutdown(_ctx); };

  protected:
    HANDLE _handle;
    OPS _ops;
    uint32_t _transfer_size;
    context::Context _ctx;

    std::function<FRAME *(HANDLE)> _get_frame_fn;
    std::function<int(HANDLE, FRAME *)> _put_frame_fn;
    std::function<HANDLE(mtl_handle, OPS *)> _create_session_fn;
    std::function<int(HANDLE)> _close_session_fn;

    Result on_establish(context::Context &ctx) override
    {
        _ctx = context::WithCancel(ctx);
        _stop = false;

        _handle = _create_session_fn(_st, &_ops);
        if (!_handle) {
            log::error("Failed to create session");
            set_state(ctx, State::closed);
            return set_result(Result::error_general_failure);
        }

        set_state(ctx, State::active);
        return set_result(Result::success);
    };

    Result on_shutdown(context::Context &ctx) override
    {
        _ctx.cancel();

        if (_handle) {
            _close_session_fn(_handle);
            _handle = nullptr;
        }
        set_state(ctx, State::closed);
        return set_result(Result::success);
    };

    Result on_receive(context::Context &ctx, void *ptr, uint32_t sz, uint32_t &sent) override
    {
        sent = std::min(_transfer_size, sz);
        // TODO: add error/warning if sent is different than _transfer_size

        FRAME *frame = NULL;
        do {
            // Get empty buffer from MTL
            frame = _get_frame_fn(_handle);
            if (!frame) {
                std::mutex mx;
                std::unique_lock lk(mx);
                _cv.wait(lk, _ctx.stop_token(), [this] { return _stop.load(); });
                _stop = false;
                if (_ctx.cancelled()) {
                    break;
                }
            }
        } while (!frame);

        if (frame) {
            // Copy data from emulated transmitter to MTL empty buffer
            mtl_memcpy(get_frame_data_ptr(frame), ptr, sent);
            // Return full buffer to MTL
            _put_frame_fn(_handle, frame);
        } else {
            sent = 0;
            return set_result(Result::error_shutdown);
        }
        return set_result(Result::success);
    };

  private:
};

class ST2110_20Tx : public ST2110Tx<st_frame, st20p_tx_handle, st20p_tx_ops> {
  public:
    ST2110_20Tx();
    ~ST2110_20Tx();

    Result configure(context::Context &ctx, const std::string &dev_port,
                     const MeshConfig_ST2110 &cfg_st2110, const MeshConfig_Video &cfg_video);

  private:
};

class ST2110_22Tx : public ST2110Tx<st_frame, st22p_tx_handle, st22p_tx_ops> {
  public:
    ST2110_22Tx();
    ~ST2110_22Tx();

    Result configure(context::Context &ctx, const std::string &dev_port,
                     const MeshConfig_ST2110 &cfg_st2110, const MeshConfig_Video &cfg_video);

  private:
};

class ST2110_30Tx : public ST2110Tx<st30_frame, st30p_tx_handle, st30p_tx_ops> {
  public:
    ST2110_30Tx();
    ~ST2110_30Tx();

    Result configure(context::Context &ctx, const std::string &dev_port,
                     const MeshConfig_ST2110 &cfg_st2110, const MeshConfig_Audio &cfg_audio);

  private:
};

} // namespace mesh::connection

#endif // ST2110TX_H
