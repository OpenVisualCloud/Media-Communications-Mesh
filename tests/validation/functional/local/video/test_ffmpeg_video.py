# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

# 1.1 Scenario: MCM to MCM - Memif (single-node) - Ffmpeg video

import pytest
import logging

from Engine.media_files import video_files_25_03

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    video_file_format_to_payload_format,
    McmConnectionType,
)

from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmMemifVideoIO
from Engine.const import (
    FFMPEG_RUN_TIMEOUT,
    DEFAULT_OUTPUT_PATH,
)

logger = logging.getLogger(__name__)


@pytest.mark.usefixtures("media_proxy")
@pytest.mark.parametrize(
    "file",
    [
        pytest.param("FullHD_59.94", marks=pytest.mark.smoke),
        *[f for f in video_files_25_03.keys() if f != "FullHD_59.94"],
    ],
)
def test_local_ffmpeg_video(
    hosts, test_config, file: str, log_path, media_path
) -> None:
    host_list = list(hosts.values())
    if len(host_list) < 1:
        pytest.skip("Local tests require at least 1 host")
    tx_host = rx_host = host_list[0]

    if hasattr(tx_host.topology.extra_info, "mcm_prefix_variables"):
        tx_prefix_variables = dict(tx_host.topology.extra_info.mcm_prefix_variables)
    else:
        tx_prefix_variables = {}
    if hasattr(rx_host.topology.extra_info, "mcm_prefix_variables"):
        rx_prefix_variables = dict(rx_host.topology.extra_info.mcm_prefix_variables)
    else:
        rx_prefix_variables = {}

    tx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = (
        tx_host.topology.extra_info.media_proxy["sdk_port"]
    )
    rx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = (
        rx_host.topology.extra_info.media_proxy["sdk_port"]
    )

    frame_rate = str(video_files_25_03[file]["fps"])
    video_size = (
        f'{video_files_25_03[file]["width"]}x{video_files_25_03[file]["height"]}'
    )
    pixel_format = video_file_format_to_payload_format(
        str(video_files_25_03[file]["file_format"])
    )
    conn_type = McmConnectionType.mpg.value

    # >>>>> MCM Tx
    mcm_tx_inp = FFmpegVideoIO(
        framerate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        stream_loop=False,
        input_path=f'{media_path}{video_files_25_03[file]["filename"]}',
    )
    mcm_tx_outp = FFmpegMcmMemifVideoIO(
        f="mcm",
        conn_type=conn_type,
        frame_rate=frame_rate,
        video_size=video_size,
        pixel_format=pixel_format,
        output_path="-",
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables=tx_prefix_variables,
        ffmpeg_path=tx_host.topology.extra_info.mcm_ffmpeg_path,
        ffmpeg_input=mcm_tx_inp,
        ffmpeg_output=mcm_tx_outp,
        yes_overwrite=False,
    )

    logger.debug(f"Tx command: {mcm_tx_ff.get_command()}")
    mcm_tx_executor = FFmpegExecutor(
        tx_host, log_path=log_path, ffmpeg_instance=mcm_tx_ff
    )

    # >>>>> MCM Rx
    mcm_rx_inp = FFmpegMcmMemifVideoIO(
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
        output_path=f'{getattr(rx_host.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH)}/test_{video_files_25_03[file]["filename"]}',
    )
    mcm_rx_ff = FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=rx_host.topology.extra_info.mcm_ffmpeg_path,
        ffmpeg_input=mcm_rx_inp,
        ffmpeg_output=mcm_rx_outp,
        yes_overwrite=True,
    )

    logger.debug(f"Rx command: {mcm_rx_ff.get_command()}")
    mcm_rx_executor = FFmpegExecutor(
        rx_host, log_path=log_path, ffmpeg_instance=mcm_rx_ff
    )

    mcm_rx_executor.start()
    mcm_tx_executor.start()
    try:
        mcm_rx_executor.wait_with_timeout(timeout=FFMPEG_RUN_TIMEOUT)
    except Exception as e:
        logging.warning(f"RX executor did not finish in time or error occurred: {e}")

    mcm_rx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    mcm_tx_executor.stop(wait=test_config.get("test_time_sec", 0.0))

    mcm_rx_executor.validate()
    mcm_tx_executor.validate()

    # TODO add validate() function to check if the output file is correct

    mcm_rx_executor.cleanup()

    assert mcm_tx_executor.is_pass is True, "TX FFmpeg process did not pass"
    assert mcm_rx_executor.is_pass is True, "RX FFmpeg process did not pass"
