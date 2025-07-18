# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import logging
import pytest
from time import sleep

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    FFmpegVideoFormat,
    video_file_format_to_payload_format,
)
from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt20pRx
from common.nicctl import Nicctl
from Engine.const import *
from Engine.media_files import video_files_25_03 as yuv_files
from Engine import rx_tx_app_connection, rx_tx_app_engine_mcm, rx_tx_app_payload

logger = logging.getLogger(__name__)


@pytest.mark.usefixtures("media_proxy")
@pytest.mark.parametrize("video_type", list(yuv_files.keys()))
def test_3_2_st2110_standalone_video(hosts, test_config, video_type, log_path):
    tx_host = hosts["mesh-agent"]
    rx_host = hosts["client"]

    nicctl = Nicctl(
        host=rx_host,
        mtl_path=rx_host.topology.extra_info.mtl_path,
    )
    vfs = nicctl.prepare_vfs_for_test(rx_host.network_interfaces[0])

    # Tx

    tx_config = test_config["tx"]
    rx_prefix_variables = test_config["rx"].get("prefix_variables", {})

    tx_executor = rx_tx_app_engine_mcm.LapkaExecutor.Tx(
        host=tx_host,
        media_path=tx_config["media_path"],
        rx_tx_app_connection=lambda: rx_tx_app_connection.St2110_20(
            remoteIpAddr=test_config.get("broadcast_ip"),
            remotePort=test_config.get("port"),
            pacing=test_config.get("pacing"),
            payloadType=test_config.get("payload_type"),
        ),
        payload_type=rx_tx_app_payload.Video,
        file_dict=yuv_files[video_type],
        file=video_type,
        log_path=log_path,
        loop=-1,
    )

    # Rx

    fps = str(yuv_files[video_type]["fps"])
    size = f"{yuv_files[video_type]['width']}x{yuv_files[video_type]['height']}"
    pixel_format = video_file_format_to_payload_format(
        yuv_files[video_type]["file_format"]
    )

    rx_ffmpeg_input = FFmpegMtlSt20pRx(
        video_size=size,
        pixel_format=pixel_format,
        fps=fps,
        timeout_s=None,
        init_retry=None,
        fb_cnt=None,
        p_rx_ip=test_config["broadcast_ip"],
        p_port=str(vfs[1]),
        p_sip=test_config["rx"]["p_sip"],
        rx_queues=None,
        tx_queues=None,
        udp_port=test_config["port"],
        payload_type=test_config["payload_type"],
        input_path="-",
    )
    rx_ffmpeg_output = FFmpegVideoIO(
        video_size=size,
        pixel_format=(
            "yuv422p10le" if pixel_format == "yuv422p10rfc4175" else pixel_format
        ),
        f=FFmpegVideoFormat.raw.value,
        output_path=f'{test_config["rx"]["filepath"]}test_{yuv_files[video_type]["filename"]}_{size}at{yuv_files[video_type]["fps"]}fps.yuv',
        pix_fmt=None,
    )
    rx_ffmpeg = FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=test_config["rx"]["ffmpeg_path"],
        ffmpeg_input=rx_ffmpeg_input,
        ffmpeg_output=rx_ffmpeg_output,
        yes_overwrite=True,
    )
    rx_executor = FFmpegExecutor(rx_host, ffmpeg_instance=rx_ffmpeg)

    tx_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    rx_executor.start()
    sleep(MTL_ESTABLISH_TIMEOUT)

    sleep(test_config["test_time"])

    tx_executor.stop()
    rx_executor.stop()
