#include "st2110rx.h"

namespace mesh::connection {

ST2110_30Rx::ST2110_30Rx() {}

ST2110_30Rx::~ST2110_30Rx()
{
    if (_ops.name)
        free((void *)_ops.name);
}

st30_frame *ST2110_30Rx::get_frame(st30p_rx_handle h) { return st30p_rx_get_frame(h); };

int ST2110_30Rx::put_frame(st30p_rx_handle h, st30_frame *f) { return st30p_rx_put_frame(h, f); };

st30p_rx_handle ST2110_30Rx::create_session(mtl_handle h, st30p_rx_ops *o)
{
    return st30p_rx_create(h, o);
};

int ST2110_30Rx::close_session(st30p_rx_handle h) { return st30p_rx_free(h); };

Result ST2110_30Rx::configure(context::Context &ctx, const std::string &dev_port,
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

    mtl_device = get_mtl_handle(dev_port, MTL_LOG_LEVEL_CRIT, cfg_st2110.local_ip_addr);
    if (mtl_device == nullptr) {
        log::error("Failed to get MTL device");
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_rx_st30_%d", session_id++);

    inet_pton(AF_INET, cfg_st2110.remote_ip_addr, _ops.port.ip_addr[MTL_PORT_P]);
    inet_pton(AF_INET, cfg_st2110.local_ip_addr, _ops.port.mcast_sip_addr[MTL_PORT_P]);
    _ops.port.udp_port[MTL_PORT_P] = cfg_st2110.local_port;

    strlcpy(_ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    _ops.port.num_port = 1;
    _ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST30;
    if (_ops.name)
        free((void *)_ops.name);
    _ops.name = strdup(session_name);
    _ops.framebuff_cnt = 4;
    _ops.fmt = mesh_audio_format_to_st_format(cfg_audio.format);
    _ops.channel = cfg_audio.channels;
    _ops.sampling = mesh_audio_sampling_to_st_sampling(cfg_audio.sample_rate);
    _ops.ptime = mesh_audio_ptime_to_st_ptime(cfg_audio.packet_time);

    log::info("ProxyContext: %s...", __func__);
    log::info("port          : %s", _ops.port.port[MTL_PORT_P]);
    log::info("ip_addr       : %d %d %d %d", _ops.port.ip_addr[MTL_PORT_P][0],
              _ops.port.ip_addr[MTL_PORT_P][2], _ops.port.ip_addr[MTL_PORT_P][2],
              _ops.port.ip_addr[MTL_PORT_P][3]);
    log::info("mcast_sip_addr: %d %d %d %d", _ops.port.mcast_sip_addr[MTL_PORT_P][0],
              _ops.port.mcast_sip_addr[MTL_PORT_P][1], _ops.port.mcast_sip_addr[MTL_PORT_P][2],
              _ops.port.mcast_sip_addr[MTL_PORT_P][3]);
    log::info("num_port      : %d", _ops.port.num_port);
    log::info("udp_port      : %d", _ops.port.udp_port[MTL_PORT_P]);
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
