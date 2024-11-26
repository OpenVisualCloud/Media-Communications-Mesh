#include "st2110.h"

namespace mesh::connection {

st_frame_fmt ST2110::mesh_video_format_to_st_format(int fmt)
{
    switch (fmt) {
    case MESH_VIDEO_PIXEL_FORMAT_NV12:
        return ST_FRAME_FMT_YUV420CUSTOM8;
    case MESH_VIDEO_PIXEL_FORMAT_YUV422P:
        return ST_FRAME_FMT_YUV422PLANAR8;
    case MESH_VIDEO_PIXEL_FORMAT_YUV422P10LE:
        return ST_FRAME_FMT_YUV422PLANAR10LE;
    case MESH_VIDEO_PIXEL_FORMAT_YUV444P10LE:
        return ST_FRAME_FMT_YUV444PLANAR10LE;
    case MESH_VIDEO_PIXEL_FORMAT_RGB8:
        return ST_FRAME_FMT_RGB8;
    default:
        return ST_FRAME_FMT_YUV422PLANAR10LE;
    }
}

st30_fmt ST2110::mesh_audio_format_to_st_format(int fmt)
{
    switch (fmt) {
    case MESH_AUDIO_FORMAT_PCM_S8:
        return ST30_FMT_PCM8;
    case MESH_AUDIO_FORMAT_PCM_S16BE:
        return ST30_FMT_PCM16;
    case MESH_AUDIO_FORMAT_PCM_S24BE:
        return ST30_FMT_PCM24;
    default:
        return ST30_FMT_MAX;
    }
}

st30_sampling ST2110::mesh_audio_sampling_to_st_sampling(int sampling)
{
    switch (sampling) {
    case MESH_AUDIO_SAMPLE_RATE_48000:
        return ST30_SAMPLING_48K;
    case MESH_AUDIO_SAMPLE_RATE_96000:
        return ST30_SAMPLING_96K;
    case MESH_AUDIO_SAMPLE_RATE_44100:
        return ST31_SAMPLING_44K;
    default:
        return ST30_SAMPLING_MAX;
    }
}

st30_ptime ST2110::mesh_audio_ptime_to_st_ptime(int ptime)
{
    switch (ptime) {
    case MESH_AUDIO_PACKET_TIME_1MS:
        return ST30_PTIME_1MS;
    case MESH_AUDIO_PACKET_TIME_125US:
        return ST30_PTIME_125US;
    case MESH_AUDIO_PACKET_TIME_250US:
        return ST30_PTIME_250US;
    case MESH_AUDIO_PACKET_TIME_333US:
        return ST30_PTIME_333US;
    case MESH_AUDIO_PACKET_TIME_4MS:
        return ST30_PTIME_4MS;
    case MESH_AUDIO_PACKET_TIME_80US:
        return ST31_PTIME_80US;
    case MESH_AUDIO_PACKET_TIME_1_09MS:
        return ST31_PTIME_1_09MS;
    case MESH_AUDIO_PACKET_TIME_0_14MS:
        return ST31_PTIME_0_14MS;
    case MESH_AUDIO_PACKET_TIME_0_09MS:
        return ST31_PTIME_0_09MS;
    default:
        return ST30_PTIME_MAX;
    }
}

void *ST2110::get_frame_data_ptr(st_frame *src) { return src->addr[0]; }
void *ST2110::get_frame_data_ptr(st30_frame *src) { return src->addr; }

int ST2110::frame_available_cb(void *ptr)
{
    auto _this = static_cast<ST2110 *>(ptr);
    if (!_this) {
        return -1;
    }
    std::unique_lock lk(_this->mx);
    _this->stop.store(true);
    _this->cv.notify_all();

    return 0;
}

void ST2110::get_mtl_dev_params(mtl_init_params &st_param, const std::string &dev_port,
                                mtl_log_level log_level,
                                const char local_ip_addr[MESH_IP_ADDRESS_SIZE])
{
    if (getenv("KAHAWAI_CFG_PATH") == NULL) {
        setenv("KAHAWAI_CFG_PATH", "/usr/local/etc/imtl.json", 0);
    }
    strlcpy(st_param.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    inet_pton(AF_INET, local_ip_addr, st_param.sip_addr[MTL_PORT_P]);
    st_param.pmd[MTL_PORT_P] = mtl_pmd_by_port_name(st_param.port[MTL_PORT_P]);
    st_param.num_ports = 1;
    st_param.flags = MTL_FLAG_BIND_NUMA;
    st_param.flags |= MTL_FLAG_TX_VIDEO_MIGRATE;
    st_param.flags |= MTL_FLAG_RX_VIDEO_MIGRATE;
    st_param.flags |= MTL_FLAG_RX_UDP_PORT_ONLY;
    st_param.pacing = ST21_TX_PACING_WAY_AUTO;
    st_param.log_level = log_level;
    st_param.priv = NULL;
    st_param.ptp_get_time_fn = NULL;
    // Native af_xdp have only 62 queues available
    if (st_param.pmd[MTL_PORT_P] == MTL_PMD_NATIVE_AF_XDP) {
        st_param.rx_queues_cnt[MTL_PORT_P] = 62;
        st_param.tx_queues_cnt[MTL_PORT_P] = 62;
    } else {
        st_param.rx_queues_cnt[MTL_PORT_P] = 128;
        st_param.tx_queues_cnt[MTL_PORT_P] = 128;
    }
    st_param.lcores = NULL;
    st_param.memzone_max = 9000;
}

mtl_handle ST2110::get_mtl_handle(const std::string &dev_port, mtl_log_level log_level,
                                  const char local_ip_addr[MESH_IP_ADDRESS_SIZE])
{
    static mtl_handle dev_handle;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    if (dev_handle) {
        return dev_handle;
    }

    mtl_init_params st_param = {0};
    get_mtl_dev_params(st_param, dev_port, log_level, local_ip_addr);
    // create device
    dev_handle = mtl_init(&st_param);
    if (!dev_handle) {
        log::error("Failed to initialize MTL device");
        return nullptr;
    }

    // start MTL device
    if (mtl_start(dev_handle) != 0) {
        log::error("Failed to start MTL device");
        return nullptr;
    }

    return dev_handle;
}

} // namespace mesh::connection
