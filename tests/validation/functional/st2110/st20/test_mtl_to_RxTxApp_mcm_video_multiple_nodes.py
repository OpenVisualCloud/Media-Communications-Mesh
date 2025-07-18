# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
import time
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
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt20pTx
from common.nicctl import Nicctl
from Engine.const import (
    DEFAULT_REMOTE_IP_ADDR,
    DEFAULT_REMOTE_PORT,
    DEFAULT_PACING,
    DEFAULT_PAYLOAD_TYPE_ST2110_20,
    DEFAULT_PIXEL_FORMAT,
    MCM_ESTABLISH_TIMEOUT,
    MTL_ESTABLISH_TIMEOUT,
    MCM_RXTXAPP_RUN_TIMEOUT,
)
from Engine.mcm_apps import get_mtl_path
from Engine.media_files import video_files_25_03 as yuv_files

logger = logging.getLogger(__name__)


@pytest.mark.parametrize("video_type", [k for k in yuv_files.keys()])
def test_st2110_rttxapp_mtl_to_mcm_video(
    build_TestApp, hosts, media_proxy, media_path, test_config, video_type, log_path
) -> None:
    
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")

    tx_host = host_list[0]
    rx_host_a = host_list[0]
    rx_host_b = host_list[1]

    tx_prefix_variables = test_config["tx"].get("prefix_variables", {})
    tx_mtl_path = get_mtl_path(tx_host)

    video_size = f'{yuv_files[video_type]["width"]}x{yuv_files[video_type]["height"]}'
    video_pixel_format = video_file_format_to_payload_format(
        str(yuv_files[video_type]["file_format"])
    )

    tx_nicctl = Nicctl(
        mtl_path=tx_mtl_path,
        host=tx_host,
    )
    tx_vfs = tx_nicctl.prepare_vfs_for_test(tx_host.network_interfaces[0])
    mtl_tx_inp = FFmpegVideoIO(
        stream_loop=-1,
        input_path=f'{test_config["tx"]["filepath"]}{yuv_files[video_type]["filename"]}',
        video_size=video_size,
        f=FFmpegVideoFormat.raw.value,
        pix_fmt=video_pixel_format,
    )
    mtl_tx_outp = FFmpegMtlSt20pTx(
        p_port=str(tx_vfs[1]),
        p_sip=test_config["tx"]["p_sip"],
        p_tx_ip=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
        udp_port=test_config.get("port", DEFAULT_REMOTE_PORT),
        payload_type=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_20),
        output_path="-",
        rx_queues=None,
        tx_queues=None,
    )
    mtl_tx_ff = FFmpeg(
        prefix_variables=tx_prefix_variables,
        ffmpeg_path=test_config["tx"]["ffmpeg_path"],
        ffmpeg_input=mtl_tx_inp,
        ffmpeg_output=mtl_tx_outp,
        yes_overwrite=False,
    )
    logger.debug(f"Tx command executed on {tx_host.name}: {mtl_tx_ff.get_command()}")
    mtl_tx_executor = FFmpegExecutor(
        tx_host, ffmpeg_instance=mtl_tx_ff, log_path=log_path
    )

    rx_connection = Engine.rx_tx_app_connection.St2110_20(
        remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
        remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
        pacing=test_config.get("pacing", DEFAULT_PACING),
        payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_20),
        transportPixelFormat=test_config.get("pixel_format", DEFAULT_PIXEL_FORMAT),
    )
    
    rx_executor_a = utils.LapkaExecutor.Rx(
        host=rx_host_a,
        media_path=media_path,
        rx_tx_app_connection=rx_connection,
        payload_type=Engine.rx_tx_app_payload.Video,
        file_dict=yuv_files[video_type],
        file=video_type,
        log_path=log_path,
    )
    
    rx_executor_b = utils.LapkaExecutor.Rx(
        host=rx_host_b,
        media_path=media_path,
        rx_tx_app_connection=rx_connection,
        payload_type=Engine.rx_tx_app_payload.Video,
        file_dict=yuv_files[video_type],
        file=video_type,
        log_path=log_path,
    )

    rx_executor_a.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    rx_executor_b.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    mtl_tx_executor.start()

    sleep(MTL_ESTABLISH_TIMEOUT)

    try:
        if rx_executor_b.process.running:
            rx_executor_b.process.wait(timeout=MCM_RXTXAPP_RUN_TIMEOUT)
    except Exception as e:
        logger.warning(f"RX executor did not finish in time or error occurred: {e}")

    rx_executor_a.stop()
    rx_executor_b.stop()
    mtl_tx_executor.stop(wait=test_config.get("test_time_sec", 0.0))

    rx_executor_a.cleanup()
    rx_executor_b.cleanup()

    assert (
        rx_executor_a.is_pass
    ), "Receiver A validation failed. Check logs for details."
    assert (
        rx_executor_b.is_pass
    ), "Receiver B validation failed. Check logs for details."
