#include "st2110tx.h"

namespace mesh::connection {

ST2110_30Tx::ST2110_30Tx()
{
    _get_frame_fn = st30p_tx_get_frame;
    _put_frame_fn = st30p_tx_put_frame;
    _create_session_fn = st30p_tx_create;
    _close_session_fn = st30p_tx_free;
}

ST2110_30Tx::~ST2110_30Tx() {}

Result ST2110_30Tx::configure(context::Context &ctx, const std::string &dev_port,
                              const MeshConfig_ST2110 &cfg_st2110,
                              const MeshConfig_Audio &cfg_audio)
{
    static int session_id;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    if (cfg_st2110.transport != MESH_CONN_TRANSPORT_ST2110_30) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    _st = get_mtl_handle(dev_port, MTL_LOG_LEVEL_CRIT, cfg_st2110.local_ip_addr);
    if (_st == nullptr) {
        log::error("Failed to get MTL device");
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st30_%d", session_id++);

    inet_pton(AF_INET, cfg_st2110.remote_ip_addr, _ops.port.dip_addr[MTL_PORT_P]);
    _ops.port.udp_port[MTL_PORT_P] = cfg_st2110.remote_port;
    strlcpy(_ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    _ops.port.udp_src_port[MTL_PORT_P] = cfg_st2110.local_port;
    _ops.port.num_port = 1;
    _ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST30;
    _ops.name = strdup(session_name);
    _ops.framebuff_cnt = 4;
    _ops.fmt = mesh_audio_format_to_st_format(cfg_audio.format);
    _ops.channel = cfg_audio.channels;
    _ops.sampling = mesh_audio_sampling_to_st_sampling(cfg_audio.sample_rate);
    _ops.ptime = mesh_audio_ptime_to_st_ptime(cfg_audio.packet_time);

    log::info("ProxyContext: %s...", __func__);
    log::info("port          : %s", _ops.port.port[MTL_PORT_P]);
    log::info("dip_addr      : %d %d %d %d", _ops.port.dip_addr[MTL_PORT_P][0],
              _ops.port.dip_addr[MTL_PORT_P][2], _ops.port.dip_addr[MTL_PORT_P][2],
              _ops.port.dip_addr[MTL_PORT_P][3]);
    log::info("num_port      : %d", _ops.port.num_port);
    log::info("udp_port      : %d", _ops.port.udp_port[MTL_PORT_P]);
    log::info("udp_src_port  : %d", _ops.port.udp_src_port[MTL_PORT_P]);
    log::info("payload_type  : %d", _ops.port.payload_type);
    log::info("name          : %s", _ops.name);
    log::info("framebuff_cnt : %d", _ops.framebuff_cnt);
    log::info("audio_fmt     : %d", _ops.fmt);
    log::info("audio_chan    : %d", _ops.channel);
    log::info("audio_sampl   : %d", _ops.sampling);
    log::info("audio_ptime   : %d", _ops.ptime);

    _ops.framebuff_size = _transfer_size =
        st30_get_packet_size(_ops.fmt, _ops.ptime, _ops.sampling, _ops.channel);
    _ops.priv = this; // app handle register to lib
    _ops.notify_frame_available = frame_available_cb;

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
