#include "st2110tx.h"

namespace mesh::connection {

ST2110_20Tx::ST2110_20Tx() {}

ST2110_20Tx::~ST2110_20Tx()
{
    if (ops.name)
        free((void *)ops.name);
}

st_frame *ST2110_20Tx::get_frame(st20p_tx_handle h) { return st20p_tx_get_frame(h); };

int ST2110_20Tx::put_frame(st20p_tx_handle h, st_frame *f) { return st20p_tx_put_frame(h, f); };

st20p_tx_handle ST2110_20Tx::create_session(mtl_handle h, st20p_tx_ops *o)
{
    return st20p_tx_create(h, o);
};

int ST2110_20Tx::close_session(st20p_tx_handle h) { return st20p_tx_free(h); };

Result ST2110_20Tx::configure(context::Context &ctx, const std::string &dev_port,
                              const MeshConfig_ST2110 &cfg_st2110,
                              const MeshConfig_Video &cfg_video)
{
    static int session_id;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    if (cfg_st2110.transport != MESH_CONN_TRANSPORT_ST2110_20) {
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

    snprintf(session_name, NAME_MAX, "mcm_tx_st20_%d", session_id++);

    inet_pton(AF_INET, cfg_st2110.remote_ip_addr, ops.port.dip_addr[MTL_PORT_P]);
    ops.port.udp_port[MTL_PORT_P] = cfg_st2110.remote_port;
    strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    ops.port.udp_src_port[MTL_PORT_P] = cfg_st2110.local_port;
    ops.port.num_port = 1;
    ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST20;
    if (ops.name)
        free((void *)ops.name);
    ops.name = strdup(session_name);
    ops.width = cfg_video.width;
    ops.height = cfg_video.height;
    ops.fps = st_frame_rate_to_st_fps(cfg_video.fps);
    ops.input_fmt = mesh_video_format_to_st_format(cfg_video.pixel_format);
    ops.transport_fmt = ST20_FMT_YUV_422_PLANAR10LE;
    ops.device = ST_PLUGIN_DEVICE_AUTO;
    ops.framebuff_cnt = 4;

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
    log::info("width         : %d", ops.width);
    log::info("height        : %d", ops.height);
    log::info("fps           : %d", ops.fps);
    log::info("transport_fmt : %d", ops.transport_fmt);
    log::info("input_fmt     : %d", ops.input_fmt);
    log::info("device        : %d", ops.device);
    log::info("framebuff_cnt : %d", ops.framebuff_cnt);

    transfer_size = st_frame_size(ops.input_fmt, ops.width, ops.height, false);
    ops.priv = this; // app handle register to lib
    ops.notify_frame_available = frame_available_cb;

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
