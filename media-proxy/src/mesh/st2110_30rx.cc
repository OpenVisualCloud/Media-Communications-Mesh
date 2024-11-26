#include "st2110rx.h"

namespace mesh::connection {

ST2110_30Rx::~ST2110_30Rx()
{
    if (ops.name)
        free((void *)ops.name);
}

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

    inet_pton(AF_INET, cfg_st2110.remote_ip_addr, ops.port.ip_addr[MTL_PORT_P]);
    inet_pton(AF_INET, cfg_st2110.local_ip_addr, ops.port.mcast_sip_addr[MTL_PORT_P]);
    ops.port.udp_port[MTL_PORT_P] = cfg_st2110.local_port;

    strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    ops.port.num_port = 1;
    ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST30;
    if (ops.name)
        free((void *)ops.name);
    ops.name = strdup(session_name);
    ops.framebuff_cnt = 4;
    ops.fmt = mesh_audio_format_to_st_format(cfg_audio.format);
    ops.channel = cfg_audio.channels;
    ops.sampling = mesh_audio_sampling_to_st_sampling(cfg_audio.sample_rate);
    ops.ptime = mesh_audio_ptime_to_st_ptime(cfg_audio.packet_time);

    log::info("ProxyContext: %s...", __func__);
    log::info("port          : %s", ops.port.port[MTL_PORT_P]);
    log::info("ip_addr       : %d %d %d %d", ops.port.ip_addr[MTL_PORT_P][0],
              ops.port.ip_addr[MTL_PORT_P][2], ops.port.ip_addr[MTL_PORT_P][2],
              ops.port.ip_addr[MTL_PORT_P][3]);
    log::info("mcast_sip_addr: %d %d %d %d", ops.port.mcast_sip_addr[MTL_PORT_P][0],
              ops.port.mcast_sip_addr[MTL_PORT_P][1], ops.port.mcast_sip_addr[MTL_PORT_P][2],
              ops.port.mcast_sip_addr[MTL_PORT_P][3]);
    log::info("num_port      : %d", ops.port.num_port);
    log::info("udp_port      : %d", ops.port.udp_port[MTL_PORT_P]);
    log::info("payload_type  : %d", ops.port.payload_type);
    log::info("name          : %s", ops.name);
    log::info("framebuff_cnt : %d", ops.framebuff_cnt);
    log::info("audio_fmt     : %d", ops.fmt);
    log::info("audio_chan    : %d", ops.channel);
    log::info("audio_sampl   : %d", ops.sampling);
    log::info("audio_ptime   : %d", ops.ptime);

    ops.framebuff_size = transfer_size =
        st30_get_packet_size(ops.fmt, ops.ptime, ops.sampling, ops.channel);
    ops.priv = this; // app handle register to lib
    ops.notify_frame_available = frame_available_cb;

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
