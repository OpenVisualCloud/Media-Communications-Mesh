/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "st2110.h"

namespace mesh::connection {

int mesh_video_format_to_st_format(int mesh_fmt, st_frame_fmt& st_fmt) {
    switch (mesh_fmt) {
    case MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE:
        st_fmt = ST_FRAME_FMT_YUV422PLANAR10LE;
        break;
    case MESH_VIDEO_PIXEL_FORMAT_V210:
        st_fmt = ST_FRAME_FMT_V210;
        break;
    case MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10:
        st_fmt = ST_FRAME_FMT_YUV422RFC4175PG2BE10;
        break;
    default:
        return -1; // Error: unknown format
    }

    return 0; // Success
}

/**
 * Converts a mesh transport video format to an ST 2110-20 format.
 *
 * This function currently supports only one specific transport format:
 * MESH_CONN_ST2110_20_TRANSPORT_FMT_YUV422_10BIT. The function uses a switch
 * statement to handle the conversion, even though there is only one case
 * supported at the moment. This design choice is intentional. By using a switch statement,
 * we can easily accommodate any future changes or requests from the customers/architects. If new
 * transport formats need to be supported in the future, we can simply add new cases to the switch
 * statement. This approach ensures that the code remains maintainable and scalable.
 */
int mesh_transport_video_format_to_st20_fmt(int transport_format, st20_fmt& st20_format) {
    switch (transport_format) {
    case MESH_CONN_ST2110_20_TRANSPORT_FMT_YUV422_10BIT:
        st20_format = ST20_FMT_YUV_422_10BIT;
        break;
    default:
        return -1; // Error: unknown format
    }

    return 0; // Success
}

int mesh_audio_format_to_st_format(int mesh_fmt, st30_fmt& st_fmt) {
    switch (mesh_fmt) {
    case MESH_AUDIO_FORMAT_PCM_S8:
        st_fmt = ST30_FMT_PCM8;
        break;
    case MESH_AUDIO_FORMAT_PCM_S16BE:
        st_fmt = ST30_FMT_PCM16;
        break;
    case MESH_AUDIO_FORMAT_PCM_S24BE:
        st_fmt = ST30_FMT_PCM24;
        break;
    default:
        return -1; // Error: unknown format
    }

    return 0; // Success
}

int mesh_audio_sampling_to_st_sampling(int sampling, st30_sampling& st_sampling) {
    switch (sampling) {
    case MESH_AUDIO_SAMPLE_RATE_48000:
        st_sampling = ST30_SAMPLING_48K;
        break;
    case MESH_AUDIO_SAMPLE_RATE_96000:
        st_sampling = ST30_SAMPLING_96K;
        break;
    case MESH_AUDIO_SAMPLE_RATE_44100:
        st_sampling = ST31_SAMPLING_44K;
        break;
    default:
        return -1; // Error: unknown sampling rate
    }

    return 0; // Success
}

int mesh_audio_ptime_to_st_ptime(int ptime, st30_ptime& st_ptime) {
    switch (ptime) {
    case MESH_AUDIO_PACKET_TIME_1MS:
        st_ptime = ST30_PTIME_1MS;
        break;
    case MESH_AUDIO_PACKET_TIME_125US:
        st_ptime = ST30_PTIME_125US;
        break;
    case MESH_AUDIO_PACKET_TIME_250US:
        st_ptime = ST30_PTIME_250US;
        break;
    case MESH_AUDIO_PACKET_TIME_333US:
        st_ptime = ST30_PTIME_333US;
        break;
    case MESH_AUDIO_PACKET_TIME_4MS:
        st_ptime = ST30_PTIME_4MS;
        break;
    case MESH_AUDIO_PACKET_TIME_80US:
        st_ptime = ST31_PTIME_80US;
        break;
    case MESH_AUDIO_PACKET_TIME_1_09MS:
        st_ptime = ST31_PTIME_1_09MS;
        break;
    case MESH_AUDIO_PACKET_TIME_0_14MS:
        st_ptime = ST31_PTIME_0_14MS;
        break;
    case MESH_AUDIO_PACKET_TIME_0_09MS:
        st_ptime = ST31_PTIME_0_09MS;
        break;
    default:
        return -1; // Error: unknown packet time
    }

    return 0; // Success
}

void *get_frame_data_ptr(st_frame *src) { return src->addr[0]; }

void *get_frame_data_ptr(st30_frame *src) { return src->addr; }

void get_mtl_dev_params(mtl_init_params& st_param, const std::string& dev_port,
                        mtl_log_level log_level, const std::string& ip_addr) {
    if (getenv("KAHAWAI_CFG_PATH") == NULL) {
        setenv("KAHAWAI_CFG_PATH", "/usr/local/etc/imtl.json", 0);
    }
    strlcpy(st_param.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
    inet_pton(AF_INET, ip_addr.c_str(), st_param.sip_addr[MTL_PORT_P]);
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

    // mtl backend supports up to 16 schedulers
    // without SHARED QUEUES flag every scheduler "gets" one queue
    // setting rx_queues_cnt/tx_queues_cnt to max supported size
    st_param.rx_queues_cnt[MTL_PORT_P] = 16;
    st_param.tx_queues_cnt[MTL_PORT_P] = 16;
    st_param.lcores = NULL;
    st_param.memzone_max = 9000;
}

mtl_handle get_mtl_device(const std::string& dev_port, mtl_log_level log_level,
                          const std::string& ip_addr) {
    static mtl_handle dev_handle;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    if (dev_handle) {
        return dev_handle;
    }

    mtl_init_params st_param = {0};
    get_mtl_dev_params(st_param, dev_port, log_level, ip_addr);
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

int mtl_get_session_id() {
    static int session_id;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);
    return session_id++;
}

} // namespace mesh::connection
