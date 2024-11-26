#include "st2110tx.h"

namespace mesh::connection {

ST2110_30Tx::~ST2110_30Tx()
{
    if (ops.name)
        free((void *)ops.name);
}

st30_frame *ST2110_30Tx::get_frame(st30p_tx_handle h)
{
    return st30p_tx_get_frame(h);
};

int ST2110_30Tx::put_frame(st30p_tx_handle h, st30_frame *f)
{
    return st30p_tx_put_frame(h, f);
};

st30p_tx_handle ST2110_30Tx::create_session(mtl_handle h, st30p_tx_ops *o)
{
    return st30p_tx_create(h, o);
};

int ST2110_30Tx::close_session(st30p_tx_handle h) { return st30p_tx_free(h); };

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

    mtl_device = get_mtl_handle(dev_port, MTL_LOG_LEVEL_CRIT, cfg_st2110.local_ip_addr);
    if (mtl_device == nullptr) {
        log::error("Failed to get MTL device");
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st30_%d", session_id++);

    inet_pton(AF_INET, cfg_st2110.remote_ip_addr, ops.port.dip_addr[MTL_PORT_P]);
    ops.port.udp_port[MTL_PORT_P] = cfg_st2110.remote_port;
    strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    ops.port.udp_src_port[MTL_PORT_P] = cfg_st2110.local_port;
    ops.port.num_port = 1;
    ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST30;
    if (ops.name)
        free((void *)ops.name);
    ops.name = strdup(session_name);
    ops.framebuff_cnt = 4;

    if(mesh_audio_format_to_st_format(cfg_audio.format, ops.fmt))
        return set_result(Result::error_bad_argument);

    ops.channel = cfg_audio.channels;
    if(mesh_audio_sampling_to_st_sampling(cfg_audio.sample_rate, ops.sampling))
        return set_result(Result::error_bad_argument);

    if(mesh_audio_ptime_to_st_ptime(cfg_audio.packet_time, ops.ptime))
        return set_result(Result::error_bad_argument);

    log::info("ProxyContext: %s...", __func__);
    log::info("port          : %s", ops.port.port[MTL_PORT_P]);
    log::info("dip_addr      : %d %d %d %d", ops.port.dip_addr[MTL_PORT_P][0],
              ops.port.dip_addr[MTL_PORT_P][2], ops.port.dip_addr[MTL_PORT_P][2],
              ops.port.dip_addr[MTL_PORT_P][3]);
    log::info("num_port      : %d", ops.port.num_port);
    log::info("udp_port      : %d", ops.port.udp_port[MTL_PORT_P]);
    log::info("udp_src_port  : %d", ops.port.udp_src_port[MTL_PORT_P]);
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
