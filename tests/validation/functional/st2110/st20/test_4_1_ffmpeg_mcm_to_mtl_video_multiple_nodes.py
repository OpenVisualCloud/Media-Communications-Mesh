# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
from time import sleep

import pytest

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    FFmpegVideoFormat,
    McmConnectionType,
    McmTransport,
    video_file_format_to_payload_format,
)
from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mcm_ffmpeg import (
    FFmpegMcmST2110VideoTx,
    FFmpegMcmST2110VideoRx,
)
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt20pRx
from common.nicctl import Nicctl
from Engine.const import (
    DEFAULT_OUTPUT_PATH,
    MAX_TEST_TIME_DEFAULT,
    MCM_ESTABLISH_TIMEOUT,
    MTL_ESTABLISH_TIMEOUT,
)
from Engine.mcm_apps import get_media_proxy_port, get_mtl_path
from Engine.media_files import video_files_25_03 as yuv_files

logger = logging.getLogger(__name__)


EARLY_STOP_THRESHOLD_PERCENTAGE = 20  # percentage of max_test_time to consider an early stop


@pytest.mark.parametrize("video_type", [k for k in yuv_files.keys() if "4K" not in k])
def test_st2110_ffmpeg_mcm_to_mtl_video(
    build_TestApp, hosts, media_proxy, media_path, test_config, video_type, log_path
) -> None:
    # media_proxy fixture used only to ensure that the media proxy is running
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    tx_host = host_list[0]
    rx_host_a = host_list[0]
    rx_host_b = host_list[1]
    rx_mtl_host = host_list[0]

    tx_prefix_variables = test_config["tx"].get("mcm_prefix_variables", {})
    rx_prefix_variables = test_config["rx"].get("mcm_prefix_variables", {})
    rx_mtl_prefix_variables = test_config["rx"].get("mtl_prefix_variables", {})
    tx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = get_media_proxy_port(tx_host)
    rx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = get_media_proxy_port(rx_host_b)
    rx_mtl_prefix_variables["MCM_MEDIA_PROXY_PORT"] = get_media_proxy_port(rx_mtl_host)

    frame_rate = str(yuv_files[video_type]["fps"])
    video_size = f'{yuv_files[video_type]["width"]}x{yuv_files[video_type]["height"]}'
    video_pixel_format = video_file_format_to_payload_format(str(yuv_files[video_type]["file_format"]))
    conn_type = McmConnectionType.st.value

    # MCM FFmpeg Tx
    mcm_tx_inp = FFmpegVideoIO(
        framerate=frame_rate,
        video_size=video_size,
        pixel_format=video_pixel_format,
        stream_loop=-1,
        input_path=f'{test_config["tx"]["filepath"]}{yuv_files[video_type]["filename"]}',
    )
    mcm_tx_outp = FFmpegMcmST2110VideoTx(
        f="mcm",
        conn_type=conn_type,
        transport=McmTransport.st20.value,
        frame_rate=frame_rate,
        ip_addr=test_config["broadcast_ip"],
        port=test_config["port"],
        video_size=video_size,
        payload_type=test_config["payload_type"],
        pixel_format=video_pixel_format,
        output_path="-",
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables=tx_prefix_variables,
        ffmpeg_path=test_config["tx"]["mcm_ffmpeg_path"],
        ffmpeg_input=mcm_tx_inp,
        ffmpeg_output=mcm_tx_outp,
        yes_overwrite=False,
    )
    logger.debug(f"MCM Tx command executed on {tx_host.name}: {mcm_tx_ff.get_command()}")
    mcm_tx_executor = FFmpegExecutor(tx_host, ffmpeg_instance=mcm_tx_ff, log_path=log_path)

    rx_mtl_path = get_mtl_path(rx_mtl_host)

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
        output_path=f'{getattr(rx_mtl_host.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH)}/test_{yuv_files[video_type]["filename"]}_{video_size}at{yuv_files[video_type]["fps"]}fps.yuv',
        pix_fmt=None,  # this is required to overwrite the default
    )
    mtl_rx_ff = FFmpeg(
        prefix_variables=rx_mtl_prefix_variables,
        ffmpeg_path=test_config["rx"]["mtl_ffmpeg_path"],
        ffmpeg_input=mtl_rx_inp,
        ffmpeg_output=mtl_rx_outp,
        yes_overwrite=True,
    )
    logger.debug(f"Mtl rx command executed on {rx_mtl_host.name}: {mtl_rx_ff.get_command()}")
    mtl_rx_executor = FFmpegExecutor(rx_mtl_host, ffmpeg_instance=mtl_rx_ff, log_path=log_path)

    # MCM FFmpeg Rx A
    mcm_rx_a_inp = FFmpegMcmST2110VideoRx(
        f="mcm",
        read_at_native_rate=True,
        conn_type=conn_type,
        transport=McmTransport.st20.value,
        frame_rate=frame_rate,
        ip_addr=test_config["broadcast_ip"],
        port=test_config["port"],
        video_size=video_size,
        payload_type=test_config["payload_type"],
        pixel_format=video_pixel_format,
        input_path="-",
    )
    mcm_rx_a_outp = FFmpegVideoIO(
        video_size=video_size,
        pixel_format=video_pixel_format,
        f=FFmpegVideoFormat.raw.value,
        output_path=f'{getattr(rx_host_b.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH)}/test_{yuv_files[video_type]["filename"]}_rx_a_{video_size}at{yuv_files[video_type]["fps"]}fps.yuv',
        pix_fmt=None,
    )
    mcm_rx_a_ff = FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=test_config["rx"]["mcm_ffmpeg_path"],
        ffmpeg_input=mcm_rx_a_inp,
        ffmpeg_output=mcm_rx_a_outp,
        yes_overwrite=True,
    )
    logger.debug(f"MCM Rx A command executed on {rx_host_a.name}: {mcm_rx_a_ff.get_command()}")
    mcm_rx_a_executor = FFmpegExecutor(rx_host_a, ffmpeg_instance=mcm_rx_a_ff, log_path=log_path)

    # MCM FFmpeg Rx B
    mcm_rx_b_inp = FFmpegMcmST2110VideoRx(
        f="mcm",
        read_at_native_rate=True,
        conn_type=conn_type,
        transport=McmTransport.st20.value,
        frame_rate=frame_rate,
        ip_addr=test_config["broadcast_ip"],
        port=test_config["port"],
        video_size=video_size,
        payload_type=test_config["payload_type"],
        pixel_format=video_pixel_format,
        input_path="-",
    )
    mcm_rx_b_outp = FFmpegVideoIO(
        video_size=video_size,
        pixel_format=video_pixel_format,
        f=FFmpegVideoFormat.raw.value,
        output_path=f'{getattr(rx_host_b.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH)}/test_{yuv_files[video_type]["filename"]}_rx_b_{video_size}at{yuv_files[video_type]["fps"]}fps.yuv',
        pix_fmt=None,
    )
    mcm_rx_b_ff = FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=test_config["rx"]["mcm_ffmpeg_path"],
        ffmpeg_input=mcm_rx_b_inp,
        ffmpeg_output=mcm_rx_b_outp,
        yes_overwrite=True,
    )
    logger.debug(f"MCM Rx B command executed on {rx_host_b.name}: {mcm_rx_b_ff.get_command()}")
    mcm_rx_b_executor = FFmpegExecutor(rx_host_b, ffmpeg_instance=mcm_rx_b_ff, log_path=log_path)

    mcm_tx_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    mcm_rx_b_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    mcm_rx_a_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    mtl_rx_executor.start()

    max_test_time = test_config.get("test_time_sec", MAX_TEST_TIME_DEFAULT)
    tx_stopping_time = mcm_tx_executor.stop(wait=max_test_time)
    logger.info(f"Tx stopping time: ~{tx_stopping_time} seconds")

    # if Tx stops in EARLY_STOP_THRESHOLD_PERCENTAGE% of the max_test_time, it is considered an early stop
    if tx_stopping_time <= (max_test_time * EARLY_STOP_THRESHOLD_PERCENTAGE / 100):
        logger.warning(
            f"It seems FFmpeg on {tx_host.name} stopped too early! Cutting down the Rx stop wait time to {tx_stopping_time} seconds."
        )
        rx_stopping_wait = tx_stopping_time
    else:
        rx_stopping_wait = max_test_time - tx_stopping_time

    # Stop all receivers
    mcm_rx_a_executor.stop(wait=rx_stopping_wait)
    mcm_rx_b_executor.stop(wait=rx_stopping_wait)
    mtl_rx_executor.stop(wait=rx_stopping_wait)

    # TODO: Add file validation to check if output files exist and have content
    logger.info("Test completed successfully - all FFmpeg processes executed")
