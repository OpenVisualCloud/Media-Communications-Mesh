# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import logging
import pytest

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    video_file_format_to_payload_format,
    McmConnectionType,
)

from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmMultipointGroupVideoIO

from Engine.media_files import yuv_files

logger = logging.getLogger(__name__)


@pytest.mark.parametrize("video_type", [k for k in yuv_files.keys()])
def test_cluster_ffmpeg_video(hosts, media_proxy, test_config, video_type: str, log_path) -> None:
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    tx_host = host_list[0]
    rx_host = host_list[1]
    tx_prefix_variables = test_config["tx"].get("prefix_variables", {})
    rx_prefix_variables = test_config["rx"].get("prefix_variables", {})
    tx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = (
        tx_host.topology.extra_info.media_proxy["sdk_port"]
    )
    rx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = (
        rx_host.topology.extra_info.media_proxy["sdk_port"]
    )

    frame_rate = str(yuv_files[video_type]["fps"])
    video_size = f'{yuv_files[video_type]["width"]}x{yuv_files[video_type]["height"]}'
    pixel_format = video_file_format_to_payload_format(
        str(yuv_files[video_type]["file_format"])
    )
    conn_type = McmConnectionType.mpg.value

    # >>>>> MCM Tx
    mcm_tx_inp = FFmpegVideoIO(
        framerate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        stream_loop=False,
        input_path=f'{test_config["tx"]["filepath"]}{yuv_files[video_type]["filename"]}',
    )
    mcm_tx_outp = FFmpegMcmMultipointGroupVideoIO(
        f="mcm",
        conn_type=conn_type,
        frame_rate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        output_path="-",
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables=tx_prefix_variables,
        ffmpeg_path=test_config["tx"]["ffmpeg_path"],
        ffmpeg_input=mcm_tx_inp,
        ffmpeg_output=mcm_tx_outp,
        yes_overwrite=False,
    )

    logger.debug(f"Tx command on {tx_host.name}: {mcm_tx_ff.get_command()}")
    mcm_tx_executor = FFmpegExecutor(tx_host, log_path=log_path, ffmpeg_instance=mcm_tx_ff)

    # >>>>> MCM Rx
    mcm_rx_inp = FFmpegMcmMultipointGroupVideoIO(
        f="mcm",
        conn_type=conn_type,
        frame_rate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        input_path="-",
    )
    mcm_rx_outp = FFmpegVideoIO(
        f="rawvideo",
        framerate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        output_path=f'{test_config["rx"]["filepath"]}test_{yuv_files[video_type]["filename"]}',
    )
    mcm_rx_ff = FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=test_config["rx"]["ffmpeg_path"],
        ffmpeg_input=mcm_rx_inp,
        ffmpeg_output=mcm_rx_outp,
        yes_overwrite=True,
    )

    logger.debug(f"Rx command on {rx_host.name}: {mcm_rx_ff.get_command()}")
    mcm_rx_executor = FFmpegExecutor(rx_host, log_path=log_path, ffmpeg_instance=mcm_rx_ff)

    mcm_rx_executor.start()
    mcm_tx_executor.start()
    mcm_rx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    mcm_tx_executor.stop(wait=test_config.get("test_time_sec", 0.0))

    # TODO: check if the output file exists and is > 0 bytes
