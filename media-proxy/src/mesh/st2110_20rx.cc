/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "st2110rx.h"

namespace mesh::connection {

st_frame *ST2110_20Rx::get_frame(st20p_rx_handle h) {
    return st20p_rx_get_frame(h);
};

int ST2110_20Rx::put_frame(st20p_rx_handle h, st_frame *f) {
    return st20p_rx_put_frame(h, f);
};

st20p_rx_handle ST2110_20Rx::create_session(mtl_handle h, st20p_rx_ops *o) {
    return st20p_rx_create(h, o);
};

int ST2110_20Rx::close_session(st20p_rx_handle h) {
    return st20p_rx_free(h);
};

Result ST2110_20Rx::configure(context::Context& ctx, const std::string& dev_port,
                              const MeshConfig_ST2110& cfg_st2110,
                              const MeshConfig_Video& cfg_video) {
    if (cfg_st2110.transport != MESH_CONN_TRANSPORT_ST2110_20) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    if (configure_common(ctx, dev_port, cfg_st2110)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    ops.port.payload_type = ST_APP_PAYLOAD_TYPE_ST20;
    ops.width = cfg_video.width;
    ops.height = cfg_video.height;
    ops.fps = st_frame_rate_to_st_fps(cfg_video.fps);

    if(mesh_transport_video_format_to_st20_fmt(cfg_st2110.transport_format, ops.transport_fmt)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    if (mesh_video_format_to_st_format(cfg_video.pixel_format, ops.output_fmt)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    ops.device = ST_PLUGIN_DEVICE_AUTO;

    transfer_size = st_frame_size(ops.output_fmt, ops.width, ops.height, false);
    if (transfer_size == 0) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    log::info("ST2110_20Rx: configure")
        ("payload_type", (int)ops.port.payload_type)
        ("width", ops.width)
        ("height", ops.height)
        ("fps", ops.fps)
        ("transport_fmt", ops.transport_fmt)
        ("output_fmt", ops.output_fmt)
        ("device", ops.device);

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
