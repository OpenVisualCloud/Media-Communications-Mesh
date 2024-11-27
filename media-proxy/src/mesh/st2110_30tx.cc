#include "st2110tx.h"

namespace mesh::connection {

st30_frame *ST2110_30Tx::get_frame(st30p_tx_handle h) {
    return st30p_tx_get_frame(h);
};

int ST2110_30Tx::put_frame(st30p_tx_handle h, st30_frame *f) {
    return st30p_tx_put_frame(h, f);
};

st30p_tx_handle ST2110_30Tx::create_session(mtl_handle h, st30p_tx_ops *o) {
    return st30p_tx_create(h, o);
};

int ST2110_30Tx::close_session(st30p_tx_handle h) {
    return st30p_tx_free(h);
};

Result ST2110_30Tx::configure(context::Context& ctx, const std::string& dev_port,
                              const MeshConfig_ST2110& cfg_st2110,
                              const MeshConfig_Audio& cfg_audio) {
    if (cfg_st2110.transport != MESH_CONN_TRANSPORT_ST2110_30) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    if (configure_common(ctx, dev_port, cfg_st2110)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST30;

    if (mesh_audio_format_to_st_format(cfg_audio.format, ops.fmt)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    ops.channel = cfg_audio.channels;
    if (mesh_audio_sampling_to_st_sampling(cfg_audio.sample_rate, ops.sampling)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    if (mesh_audio_ptime_to_st_ptime(cfg_audio.packet_time, ops.ptime)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    log::info("ST2110_30Tx: configure")
        ("payload_type", (int)ops.port.payload_type)
        ("audio_fmt", ops.fmt)
        ("audio_chan", ops.channel)
        ("audio_sampl", ops.sampling)
        ("audio_ptime", ops.ptime);

    ops.framebuff_size = transfer_size =
        st30_get_packet_size(ops.fmt, ops.ptime, ops.sampling, ops.channel);
    if (transfer_size == 0) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
