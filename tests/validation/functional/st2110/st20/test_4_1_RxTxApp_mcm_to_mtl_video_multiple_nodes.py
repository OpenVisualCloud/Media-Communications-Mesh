# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
from time import sleep

import pytest

import Engine.rx_tx_app_connection
import Engine.rx_tx_app_engine_mcm as utils
import Engine.rx_tx_app_payload

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    FFmpegVideoFormat,
    video_file_format_to_payload_format,
)
from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt20pRx
from common.nicctl import Nicctl
from Engine.const import (
    DEFAULT_LOOP_COUNT,
    DEFAULT_REMOTE_IP_ADDR,
    DEFAULT_REMOTE_PORT,
    DEFAULT_PACING,
    DEFAULT_PAYLOAD_TYPE_ST2110_20,
    DEFAULT_PIXEL_FORMAT,
    MCM_ESTABLISH_TIMEOUT,
    MTL_ESTABLISH_TIMEOUT,
    MCM_RXTXAPP_RUN_TIMEOUT,
)
from Engine.mcm_apps import get_media_proxy_port, get_mtl_path
from Engine.media_files import video_files_25_03 as yuv_files

logger = logging.getLogger(__name__)


@pytest.mark.parametrize("video_type", [k for k in yuv_files.keys() if "4K" not in k])
def test_st2110_rttxapp_mcm_to_mtl_video(
    build_TestApp, hosts, media_proxy, media_path, test_config, video_type, log_path
) -> None:
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    tx_host = host_list[0]
    rx_host_a = host_list[0]
    rx_host_b = host_list[1]
    rx_mtl_host = host_list[0]

    tx_executor = utils.LapkaExecutor.Tx(
        host=tx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.St2110_20(
            remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
            remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
            pacing=test_config.get("pacing", DEFAULT_PACING),
            payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_20),
            transportPixelFormat=test_config.get("pixel_format", DEFAULT_PIXEL_FORMAT),
        ),
        payload_type=Engine.rx_tx_app_payload.Video,
        file_dict=yuv_files[video_type],
        file=video_type,
        log_path=log_path,
        loop=DEFAULT_LOOP_COUNT,
    )

    rx_prefix_variables = test_config["rx"].get("prefix_variables", {})
    rx_mtl_path = get_mtl_path(rx_mtl_host)

    frame_rate = str(yuv_files[video_type]["fps"])
    video_size = f'{yuv_files[video_type]["width"]}x{yuv_files[video_type]["height"]}'
    video_pixel_format = video_file_format_to_payload_format(str(yuv_files[video_type]["file_format"]))

    rx_nicctl = Nicctl(
        mtl_path=rx_mtl_path,
        host=rx_mtl_host,
    )
    rx_vfs = rx_nicctl.prepare_vfs_for_test(rx_mtl_host.network_interfaces[0])
    mtl_rx_inp = FFmpegMtlSt20pRx(
        video_size=video_size,
        pixel_format=video_pixel_format,
        fps=frame_rate,
        timeout_s=None,
        init_retry=None,
        fb_cnt=None,
        p_rx_ip=test_config["broadcast_ip"],
        p_port=str(rx_vfs[1]),
        p_sip=test_config["rx"]["p_sip"],
        rx_queues=None,
        tx_queues=None,
        udp_port=test_config["port"],
        payload_type=test_config["payload_type"],
        input_path="-",
    )
    mtl_rx_outp = FFmpegVideoIO(
        video_size=video_size,
        pixel_format=("yuv422p10le" if video_pixel_format == "yuv422p10rfc4175" else video_pixel_format),
        f=FFmpegVideoFormat.raw.value,
        output_path=f'{test_config["rx"]["filepath"]}test_{yuv_files[video_type]["filename"]}_{video_size}at{yuv_files[video_type]["fps"]}fps.yuv',
        pix_fmt=None,  # this is required to overwrite the default
    )
    mtl_rx_ff = FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=test_config["rx"]["ffmpeg_path"],
        ffmpeg_input=mtl_rx_inp,
        ffmpeg_output=mtl_rx_outp,
        yes_overwrite=True,
    )
    logger.debug(f"Mtl rx command executed on {rx_mtl_host.name}: {mtl_rx_ff.get_command()}")
    mtl_rx_executor = FFmpegExecutor(rx_mtl_host, ffmpeg_instance=mtl_rx_ff, log_path=log_path)

    rx_executor_a = utils.LapkaExecutor.Rx(
        host=rx_host_a,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.St2110_20(
            remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
            remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
            pacing=test_config.get("pacing", DEFAULT_PACING),
            payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_20),
            transportPixelFormat=test_config.get("pixel_format", DEFAULT_PIXEL_FORMAT),
        ),
        payload_type=Engine.rx_tx_app_payload.Video,
        file_dict=yuv_files[video_type],
        file=video_type,
        log_path=log_path,
    )

    rx_executor_b = utils.LapkaExecutor.Rx(
        host=rx_host_b,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.St2110_20(
            remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
            remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
            pacing=test_config.get("pacing", DEFAULT_PACING),
            payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_20),
            transportPixelFormat=test_config.get("pixel_format", DEFAULT_PIXEL_FORMAT),
        ),
        payload_type=Engine.rx_tx_app_payload.Video,
        file_dict=yuv_files[video_type],
        file=video_type,
        log_path=log_path,
    )

    tx_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    rx_executor_b.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    rx_executor_a.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    mtl_rx_executor.start()

    try:
        if rx_executor_a.process.running:
            rx_executor_a.process.wait(timeout=MCM_RXTXAPP_RUN_TIMEOUT)
    except Exception as e:
        logger.warning(f"RX executor did not finish in time or error occurred: {e}")

    rx_executor_a.stop()
    rx_executor_b.stop()
    mtl_rx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    tx_executor.stop()

    assert rx_executor_a.is_pass, "Receiver A validation failed. Check logs for details."
    assert rx_executor_b.is_pass, "Receiver B validation failed. Check logs for details."
