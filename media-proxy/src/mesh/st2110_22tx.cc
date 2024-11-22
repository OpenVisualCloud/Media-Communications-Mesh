#include "st2110tx.h"

namespace mesh::connection {

ST2110_22Tx::ST2110_22Tx() {}

ST2110_22Tx::~ST2110_22Tx() {}

st_frame *ST2110_22Tx::get_frame(st22p_tx_handle h) { return st22p_tx_get_frame(h); };

int ST2110_22Tx::put_frame(st22p_tx_handle h, st_frame *f) { return st22p_tx_put_frame(h, f); };

st22p_tx_handle ST2110_22Tx::create_session(mtl_handle h, st22p_tx_ops *o)
{
    return st22p_tx_create(h, o);
};

int ST2110_22Tx::close_session(st22p_tx_handle h) { return st22p_tx_free(h); };

Result ST2110_22Tx::configure(context::Context &ctx, const std::string &dev_port,
                              const MeshConfig_ST2110 &cfg_st2110,
                              const MeshConfig_Video &cfg_video)
{
    static int session_id;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    if (cfg_st2110.transport != MESH_CONN_TRANSPORT_ST2110_22) {
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

    snprintf(session_name, NAME_MAX, "mcm_tx_st22_%d", session_id++);

    inet_pton(AF_INET, cfg_st2110.remote_ip_addr, _ops.port.dip_addr[MTL_PORT_P]);
    _ops.port.udp_port[MTL_PORT_P] = cfg_st2110.remote_port;
    strlcpy(_ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    _ops.port.udp_src_port[MTL_PORT_P] = cfg_st2110.local_port;
    _ops.port.num_port = 1;
    _ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST22;
    _ops.name = strdup(session_name);
    _ops.width = cfg_video.width;
    _ops.height = cfg_video.height;
    _ops.fps = st_frame_rate_to_st_fps(cfg_video.fps);
    _ops.input_fmt = mesh_video_format_to_st_format(cfg_video.pixel_format);
    _ops.device = ST_PLUGIN_DEVICE_AUTO;
    _ops.framebuff_cnt = 4;
    _ops.pack_type = ST22_PACK_CODESTREAM;
    _ops.codec = ST22_CODEC_JPEGXS;
    _ops.quality = ST22_QUALITY_MODE_SPEED;
    _ops.codec_thread_cnt = 0;
    _ops.codestream_size = _ops.width * _ops.height * 3 / 8;

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
    log::info("width         : %d", _ops.width);
    log::info("height        : %d", _ops.height);
    log::info("fps           : %d", _ops.fps);
    log::info("input_fmt     : %d", _ops.input_fmt);
    log::info("device        : %d", _ops.device);
    log::info("framebuff_cnt : %d", _ops.framebuff_cnt);

    _transfer_size = st_frame_size(_ops.input_fmt, _ops.width, _ops.height, false);
    _ops.priv = this; // app handle register to lib
    _ops.notify_frame_available = frame_available_cb;

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
