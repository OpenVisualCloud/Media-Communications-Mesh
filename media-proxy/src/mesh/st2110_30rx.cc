#include "st2110rx.h"

namespace mesh::connection {

st30_frame *ST2110_30Rx::get_frame(st30p_rx_handle h)
{
    return st30p_rx_get_frame(h);
};

int ST2110_30Rx::put_frame(st30p_rx_handle h, st30_frame *f)
{
    return st30p_rx_put_frame(h, f);
};

st30p_rx_handle ST2110_30Rx::create_session(mtl_handle h, st30p_rx_ops *o)
{
    return st30p_rx_create(h, o);
};

int ST2110_30Rx::close_session(st30p_rx_handle h)
{
    return st30p_rx_free(h);
};

Result ST2110_30Rx::configure(context::Context &ctx, const std::string &dev_port,
                              const MeshConfig_ST2110 &cfg_st2110,
                              const MeshConfig_Audio &cfg_audio)
{
    set_state(ctx, State::not_configured);

    if (cfg_st2110.transport != MESH_CONN_TRANSPORT_ST2110_30)
        return set_result(Result::error_bad_argument);

    if (configure_common(ctx, dev_port, cfg_st2110))
        return set_result(Result::error_bad_argument);

    ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST30;

    if (mesh_audio_format_to_st_format(cfg_audio.format, ops.fmt))
        return set_result(Result::error_bad_argument);

    ops.channel = cfg_audio.channels;
    if (mesh_audio_sampling_to_st_sampling(cfg_audio.sample_rate, ops.sampling))
        return set_result(Result::error_bad_argument);

    if (mesh_audio_ptime_to_st_ptime(cfg_audio.packet_time, ops.ptime))
        return set_result(Result::error_bad_argument);

    log::info("ST2110_22Rx: configure")
        ("payload_type", (int)ops.port.payload_type)
        ("audio_fmt", ops.fmt)
        ("audio_chan", ops.channel)
        ("audio_sampl", ops.sampling)
        ("audio_ptime", ops.ptime);

    ops.framebuff_size = transfer_size =
        st30_get_packet_size(ops.fmt, ops.ptime, ops.sampling, ops.channel);
    if (transfer_size == 0)
        return set_result(Result::error_bad_argument);

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
