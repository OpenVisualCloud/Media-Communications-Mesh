# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
import time
import pytest

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor, no_proxy_to_prefix_variables
from common.ffmpeg_handler.ffmpeg_enums import (
    FFmpegVideoFormat, McmConnectionType, McmTransport,
    video_file_format_to_payload_format)
from common.ffmpeg_handler.ffmpeg_io import FFmpegVideoIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmST2110VideoTx
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt20pRx

from ....Engine.media_files import yuv_files

from common.nicctl import Nicctl

logger = logging.getLogger(__name__)


@pytest.mark.parametrize("video_type", [k for k in yuv_files.keys()])
def test_st2110_ffmpeg_mcm_to_mtl_video(media_proxy, hosts, test_config, video_type: str) -> None:
    # media_proxy fixture used only to ensure that the media proxy is running
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    # TX configuration
    tx_host = host_list[0]
    tx_prefix_variables = test_config["tx"].get("prefix_variables", {})
    tx_prefix_variables = no_proxy_to_prefix_variables(tx_host, tx_prefix_variables)
    tx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = tx_host.topology.extra_info.media_proxy.get("sdk_port")
    logger.debug(f"tx_prefix_variables: {tx_prefix_variables}")

    # RX configuration
    rx_host = host_list[1]
    rx_prefix_variables = test_config["rx"].get("prefix_variables", {})
    rx_prefix_variables = no_proxy_to_prefix_variables(rx_host, rx_prefix_variables)
    rx_mtl_path = rx_host.topology.extra_info.mtl_path

    # Video configuration
    frame_rate = str(yuv_files[video_type]["fps"])
    video_size = f'{yuv_files[video_type]["width"]}x{yuv_files[video_type]["height"]}'
    video_pixel_format = video_file_format_to_payload_format(str(yuv_files[video_type]["file_format"]))
    conn_type = McmConnectionType.st.value

    # Prepare Rx VFs
    rx_nicctl = Nicctl(
        host = rx_host,
        mtl_path = rx_mtl_path,
    )
    rx_vfs = rx_nicctl.prepare_vfs_for_test(rx_host.network_interfaces[0])

    # MCM Tx
    mcm_tx_inp = FFmpegVideoIO(
        stream_loop = -1,
        input_path = f'{test_config["tx"]["filepath"]}{yuv_files[video_type]["filename"]}',
        video_size = video_size,
        f = FFmpegVideoFormat.raw.value,
        pix_fmt = video_pixel_format,
    )
    mcm_tx_outp = FFmpegMcmST2110VideoTx(
        conn_type = conn_type,
        transport = McmTransport.st20.value,
        frame_rate = frame_rate,
        ip_addr = test_config["broadcast_ip"],
        port = test_config["port"],
        video_size = video_size,
        payload_type = test_config["payload_type"],
        pixel_format = video_pixel_format,
        output_path = "-",

        urn = None,
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables = tx_prefix_variables,
        ffmpeg_path = f'{test_config["tx"]["ffmpeg_path"]}',
        ffmpeg_input = mcm_tx_inp,
        ffmpeg_output = mcm_tx_outp,
        yes_overwrite = False,
    )
    logger.debug(f"Tx command executed on {tx_host.name}: {mcm_tx_ff.get_command()}")
    mcm_tx_executor = FFmpegExecutor(tx_host, ffmpeg_instance = mcm_tx_ff)

    # MTL Rx
    mtl_rx_inp = FFmpegMtlSt20pRx(
        # FFmpegMtlSt20pRx:
        video_size = video_size,
        pixel_format = video_pixel_format,
        fps = frame_rate,
        timeout_s = None,
        init_retry = None,
        fb_cnt = None,
        #FFmpegMtlCommonRx:
        p_rx_ip = test_config["broadcast_ip"],
        # FFmpegMtlCommonIO:
        p_port = str(rx_vfs[0]),
        p_sip = test_config["rx"]["p_sip"],
        rx_queues = None,
        tx_queues = None,
        udp_port = test_config["port"],
        payload_type = test_config["payload_type"],
        # FFmpegIO:
        input_path = "-",
    )
    mtl_rx_outp = FFmpegVideoIO(
        video_size = video_size,
        pixel_format = "yuv422p10le" if video_pixel_format == "yuv422p10rfc4175" else video_pixel_format,
        f = FFmpegVideoFormat.raw.value,
        output_path = f'{test_config["rx"]["filepath"]}test_{yuv_files[video_type]["filename"]}_{video_size}at{yuv_files[video_type]["fps"]}fps.yuv',
        pix_fmt = None,  # this is required to overwrite the default
    )
    mtl_rx_ff = FFmpeg(
        prefix_variables = rx_prefix_variables,
        ffmpeg_path = test_config["rx"]["ffmpeg_path"],
        ffmpeg_input = mtl_rx_inp,
        ffmpeg_output = mtl_rx_outp,
        yes_overwrite = True,
    )
    logger.debug(f"Rx command executed on {rx_host.name}: {mtl_rx_ff.get_command()}")
    mtl_rx_executor = FFmpegExecutor(rx_host, ffmpeg_instance = mtl_rx_ff)

    time.sleep(2) # wait for media_proxy to start
    mtl_rx_executor.start()
    mcm_tx_executor.start()
    time.sleep(2) # ensure executors are started
    mcm_tx_executor.stop(wait=test_config.get("test_time_sec"))
    mtl_rx_executor.stop(wait=test_config.get("test_time_sec"))
