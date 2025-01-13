/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "st2110tx.h"

namespace mesh::connection {

st_frame *ST2110_22Tx::get_frame(st22p_tx_handle h) {
    return st22p_tx_get_frame(h);
};

int ST2110_22Tx::put_frame(st22p_tx_handle h, st_frame *f) {
    return st22p_tx_put_frame(h, f);
};

st22p_tx_handle ST2110_22Tx::create_session(mtl_handle h, st22p_tx_ops *o) {
    return st22p_tx_create(h, o);
};

int ST2110_22Tx::close_session(st22p_tx_handle h) {
    return st22p_tx_free(h);
};

Result ST2110_22Tx::configure(context::Context& ctx, const std::string& dev_port,
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

    if (mesh_video_format_to_st_format(cfg_video.pixel_format, ops.input_fmt)) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }
    if(ops.input_fmt != ST_FRAME_FMT_YUV422PLANAR10LE) {
        set_state(ctx, State::not_configured);
        log::error("ST2110_22Tx: unsupported format")
                  ("expected", ST_FRAME_FMT_YUV422PLANAR10LE)
                  ("provided", ops.input_fmt);
        return set_result(Result::error_not_supported);
    }

    ops.device = ST_PLUGIN_DEVICE_AUTO;
    ops.pack_type = ST22_PACK_CODESTREAM;
    ops.codec = ST22_CODEC_JPEGXS;
    ops.quality = ST22_QUALITY_MODE_SPEED;
    ops.codec_thread_cnt = 0;
    ops.codestream_size = ops.width * ops.height * 3 / 8;

    log::info("ST2110_22Tx: configure")
        ("payload_type", (int)ops.port.payload_type)
        ("width", ops.width)
        ("height", ops.height)
        ("fps", ops.fps)
        ("input_fmt", ops.input_fmt)
        ("device", ops.device);

    transfer_size = st_frame_size(ops.input_fmt, ops.width, ops.height, false);
    if (transfer_size == 0) {
        set_state(ctx, State::not_configured);
        return set_result(Result::error_bad_argument);
    }

    set_state(ctx, State::configured);
    return set_result(Result::success);
}

} // namespace mesh::connection
