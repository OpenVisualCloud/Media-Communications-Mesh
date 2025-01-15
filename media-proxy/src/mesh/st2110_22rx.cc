/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "st2110rx.h"

namespace mesh::connection {

st_frame *ST2110_22Rx::get_frame(st22p_rx_handle h) {
    return st22p_rx_get_frame(h);
};

int ST2110_22Rx::put_frame(st22p_rx_handle h, st_frame *f) {
    return st22p_rx_put_frame(h, f);
};

st22p_rx_handle ST2110_22Rx::create_session(mtl_handle h, st22p_rx_ops *o) {
    return st22p_rx_create(h, o);
};

int ST2110_22Rx::close_session(st22p_rx_handle h) {
    return st22p_rx_free(h);
};

Result ST2110_22Rx::configure(context::Context& ctx, const std::string& dev_port,
                              const MeshConfig_ST2110& cfg_st2110,
                              const MeshConfig_Video& cfg_video) {
    if (cfg_st2110.transport != MESH_CONN_TRANSPORT_ST2110_22) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    if (configure_common(ctx, dev_port, cfg_st2110)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST22;
    ops.width = cfg_video.width;
    ops.height = cfg_video.height;
    ops.fps = st_frame_rate_to_st_fps(cfg_video.fps);

    if (mesh_video_format_to_st_format(cfg_video.pixel_format, ops.output_fmt)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }
    if(ops.output_fmt != ST_FRAME_FMT_YUV422PLANAR10LE) {
        set_state(ctx, State::not_configured);
        log::error("ST2110_22Rx: unsupported format")
                  ("expected", ST_FRAME_FMT_YUV422PLANAR10LE)
                  ("provided", ops.output_fmt);
        return set_result(Result::error_not_supported);
    }

    ops.device = ST_PLUGIN_DEVICE_AUTO;
    ops.pack_type = ST22_PACK_CODESTREAM;
    ops.codec = ST22_CODEC_JPEGXS;
    ops.codec_thread_cnt = 0;
    ops.max_codestream_size = 0;

    log::info("ST2110_22Rx: configure")
        ("payload_type", (int)ops.port.payload_type)
        ("width", ops.width)
        ("height", ops.height)
        ("fps", ops.fps)
        ("output_fmt", ops.output_fmt)
        ("device", ops.device);

    transfer_size = st_frame_size(ops.output_fmt, ops.width, ops.height, false);
    if (transfer_size == 0) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
