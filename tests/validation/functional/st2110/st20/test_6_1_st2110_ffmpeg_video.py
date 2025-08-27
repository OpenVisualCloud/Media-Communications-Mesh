# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import os
import pytest
from time import sleep


from common.ffmpeg_handler import ffmpeg_enums
from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmST2110VideoRx
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt20pTx
from Engine.const import MCM_ESTABLISH_TIMEOUT, MTL_ESTABLISH_TIMEOUT
from Engine.media_files import video_files_25_03 as video_files
from common.nicctl import Nicctl


@pytest.mark.usefixtures("media_proxy")
@pytest.mark.parametrize("video_file", video_files)
def test_6_1_st2110_ffmpeg_video(hosts, test_config, video_file, log_path):
    video_file = video_files[video_file]

    tx_host = hosts["mesh-agent"]
    rx_a_host = hosts["mesh-agent"]
    rx_b_host = hosts["client"]

    video_size = f'{video_file["width"]}x{video_file["height"]}'
    video_pixel_format = ffmpeg_enums.video_file_format_to_payload_format(video_file["file_format"])
    video_frame_rate = video_file["fps"]

    # Host A --- MTL FFmpeg Tx

    tx_input_path = tx_host.topology.extra_info.input_path
    tx_prefix_variables = dict(test_config["tx"]["prefix_variables"])
    tx_mtl_ffmpeg_path = tx_host.topology.extra_info.mtl_ffmpeg_path
    tx_mtl_ffmpeg_lib_path = tx_host.topology.extra_info.mtl_ffmpeg_lib_path
    tx_mtl_path = tx_host.topology.extra_info.mtl_path

    tx_prefix_variables["LD_LIBRARY_PATH"] = tx_mtl_ffmpeg_lib_path

    tx_nicctl = Nicctl(
        mtl_path=tx_mtl_path,
        host=tx_host,
    )
    tx_vfs = tx_nicctl.prepare_vfs_for_test(
        nic=tx_host.network_interfaces[0],
    )

    mtl_tx_input = FFmpegVideoIO(
        video_size=video_size,
        f=ffmpeg_enums.FFmpegVideoFormat.raw.value,
        pix_fmt=video_pixel_format,
        stream_loop=-1,
        input_path=os.path.join(tx_input_path, video_file["filename"]),
    )
    mtl_tx_output = FFmpegMtlSt20pTx(
        p_tx_ip=test_config["broadcast_ip"],
        p_port=tx_vfs[0],
        p_sip=test_config["tx"]["p_sip"],
        udp_port=test_config["port"],
        payload_type=test_config["payload_type"],
        rx_queues=None,
        tx_queues=None,
        output_path="-",
    )
    mtl_tx_ffmpeg = FFmpeg(
        prefix_variables=tx_prefix_variables,
        ffmpeg_path=tx_mtl_ffmpeg_path,
        ffmpeg_input=mtl_tx_input,
        ffmpeg_output=mtl_tx_output,
    )
    mtl_tx_ffmpeg_executor = FFmpegExecutor(
        host=tx_host,
        ffmpeg_instance=mtl_tx_ffmpeg,
        log_path=log_path,
    )

    # Host A --- MCM FFmpeg Rx

    rx_a_output_path = rx_a_host.topology.extra_info.output_path
    rx_a_prefix_variables = dict(test_config["rx"]["prefix_variables"])
    rx_a_mcm_ffmpeg_path = rx_a_host.topology.extra_info.mcm_ffmpeg_path
    rx_a_mcm_ffmpeg_lib_path = rx_a_host.topology.extra_info.mcm_ffmpeg_lib_path

    rx_a_prefix_variables["LD_LIBRARY_PATH"] = rx_a_mcm_ffmpeg_lib_path

    mcm_rx_a_input = FFmpegMcmST2110VideoRx(
        ip_addr=test_config["broadcast_ip"],
        transport=ffmpeg_enums.McmTransport.st20.value,
        payload_type=test_config["payload_type"],
        video_size=video_size,
        pixel_format=video_pixel_format,
        frame_rate=video_frame_rate,
        conn_type=ffmpeg_enums.McmConnectionType.st.value,
        port=test_config["port"],
        read_at_native_rate=True,
        input_path="-",
    )
    mcm_rx_a_output = FFmpegVideoIO(
        video_size=video_size,
        f=ffmpeg_enums.FFmpegVideoFormat.raw.value,
        pix_fmt=video_pixel_format,
        output_path=os.path.join(rx_a_output_path, video_file["filename"]),
    )
    mcm_rx_a_ffmpeg = FFmpeg(
        prefix_variables=rx_a_prefix_variables,
        ffmpeg_path=rx_a_mcm_ffmpeg_path,
        ffmpeg_input=mcm_rx_a_input,
        ffmpeg_output=mcm_rx_a_output,
        yes_overwrite=True,
    )
    mcm_rx_a_ffmpeg_executor = FFmpegExecutor(
        host=rx_a_host,
        ffmpeg_instance=mcm_rx_a_ffmpeg,
        log_path=log_path,
    )

    # Host B --- MCM FFmpeg Rx

    rx_b_output_path = rx_b_host.topology.extra_info.output_path
    rx_b_prefix_variables = dict(test_config["rx"]["prefix_variables"])
    rx_b_mcm_ffmpeg_path = rx_b_host.topology.extra_info.mcm_ffmpeg_path
    rx_b_mcm_ffmpeg_lib_path = rx_b_host.topology.extra_info.mcm_ffmpeg_lib_path

    rx_b_prefix_variables["LD_LIBRARY_PATH"] = rx_b_mcm_ffmpeg_lib_path

    mcm_rx_b_input = FFmpegMcmST2110VideoRx(
        ip_addr=test_config["broadcast_ip"],
        transport=ffmpeg_enums.McmTransport.st20.value,
        payload_type=test_config["payload_type"],
        video_size=video_size,
        pixel_format=video_pixel_format,
        frame_rate=video_frame_rate,
        conn_type=ffmpeg_enums.McmConnectionType.st.value,
        port=test_config["port"],
        read_at_native_rate=True,
        input_path="-",
    )
    mcm_rx_b_output = FFmpegVideoIO(
        video_size=video_size,
        f=ffmpeg_enums.FFmpegVideoFormat.raw.value,
        pix_fmt=video_pixel_format,
        output_path=os.path.join(rx_b_output_path, video_file["filename"]),
    )
    mcm_rx_b_ffmpeg = FFmpeg(
        prefix_variables=rx_b_prefix_variables,
        ffmpeg_path=rx_b_mcm_ffmpeg_path,
        ffmpeg_input=mcm_rx_b_input,
        ffmpeg_output=mcm_rx_b_output,
        yes_overwrite=True,
    )
    mcm_rx_b_ffmpeg_executor = FFmpegExecutor(
        host=rx_b_host,
        ffmpeg_instance=mcm_rx_b_ffmpeg,
        log_path=log_path,
    )

    mtl_tx_ffmpeg_executor.start()
    sleep(MTL_ESTABLISH_TIMEOUT)

    mcm_rx_a_ffmpeg_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    mcm_rx_b_ffmpeg_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    sleep(test_config["test_time"])

    mtl_tx_ffmpeg_executor.stop()
    mcm_rx_a_ffmpeg_executor.stop()
    mcm_rx_b_ffmpeg_executor.stop()
