# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
from time import sleep

import pytest

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    FFmpegAudioRate,
    McmConnectionType,
    PacketTime,
    audio_file_format_to_format_dict,
    audio_channel_number_to_layout,
)
from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mcm_ffmpeg import (
    FFmpegMcmST2110AudioTx,
    FFmpegMcmST2110AudioRx,
)
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt30pRx
from common.nicctl import Nicctl
from Engine.const import (
    DEFAULT_OUTPUT_PATH,
    MAX_TEST_TIME_DEFAULT,
    MCM_ESTABLISH_TIMEOUT,
    MTL_ESTABLISH_TIMEOUT,
)
from Engine.mcm_apps import get_media_proxy_port, get_mtl_path
from Engine.media_files import audio_files_25_03 as audio_files

logger = logging.getLogger(__name__)

EARLY_STOP_THRESHOLD_PERCENTAGE = 20  # percentage of max_test_time to consider an early stop


@pytest.mark.parametrize("audio_type", [k for k in audio_files.keys() if "PCM8" not in k])
def test_st2110_ffmpeg_mcm_to_mtl_audio(
    build_TestApp, hosts, media_proxy, media_path, test_config, audio_type, log_path
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

    audio_format = audio_file_format_to_format_dict(str(audio_files[audio_type]["format"]))
    audio_channel_layout = audio_files[audio_type].get(
        "channel_layout",
        audio_channel_number_to_layout(int(audio_files[audio_type]["channels"])),
    )

    # Set audio sample rate based on the audio file sample rate
    audio_sample_rate = int(audio_files[audio_type]["sample_rate"])
    if audio_sample_rate not in [ar.value for ar in FFmpegAudioRate]:
        raise Exception(f"Not expected audio sample rate of {audio_files[audio_type]['sample_rate']}!")

    conn_type = McmConnectionType.st.value

    # MCM FFmpeg Tx
    mcm_tx_inp = FFmpegAudioIO(
        ar=audio_sample_rate,
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        stream_loop=-1,
        input_path=f'{test_config["tx"]["filepath"]}{audio_files[audio_type]["filename"]}',
    )
    mcm_tx_outp = FFmpegMcmST2110AudioTx(
        f=audio_format["mcm_f"],
        conn_type=conn_type,
        payload_type=test_config["payload_type"],
        channels=audio_files[audio_type]["channels"],
        sample_rate=audio_sample_rate,
        ptime=PacketTime.pt_1ms.value,
        ip_addr=test_config["broadcast_ip"],
        port=test_config["port"],
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
    mtl_rx_inp = FFmpegMtlSt30pRx(
        sample_rate=audio_sample_rate,
        channels=int(audio_files[audio_type]["channels"]),
        pcm_fmt=audio_format["mtl_pcm_fmt"],
        ptime=PacketTime.pt_1ms.value,
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
    mtl_rx_outp = FFmpegAudioIO(
        ar=audio_sample_rate,
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        channel_layout=audio_channel_layout,
        output_path=f'{getattr(rx_host_b.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH)}/test_{audio_files[audio_type]["filename"]}_{audio_files[audio_type]["channels"]}ch_{audio_files[audio_type]["sample_rate"]}Hz.{audio_format["ffmpeg_f"]}',
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
    mcm_rx_a_inp = FFmpegMcmST2110AudioRx(
        f=audio_format["mcm_f"],
        conn_type=conn_type,
        ip_addr=test_config["broadcast_ip"],
        payload_type=test_config["payload_type"],
        channels=audio_files[audio_type]["channels"],
        sample_rate=audio_sample_rate,
        ptime=PacketTime.pt_1ms.value,
        port=test_config["port"],
        input_path="-",
    )
    mcm_rx_a_outp = FFmpegAudioIO(
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        ar=audio_sample_rate,
        channel_layout=audio_channel_layout,
        output_path=f'{getattr(rx_host_b.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH)}/test_{audio_files[audio_type]["filename"]}_rx_a_{audio_files[audio_type]["channels"]}ch_{audio_files[audio_type]["sample_rate"]}Hz.{audio_format["ffmpeg_f"]}',
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
    mcm_rx_b_inp = FFmpegMcmST2110AudioRx(
        f=audio_format["mcm_f"],
        conn_type=conn_type,
        ip_addr=test_config["broadcast_ip"],
        payload_type=test_config["payload_type"],
        channels=audio_files[audio_type]["channels"],
        sample_rate=audio_sample_rate,
        ptime=PacketTime.pt_1ms.value,
        port=test_config["port"],
        input_path="-",
    )
    mcm_rx_b_outp = FFmpegAudioIO(
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        ar=audio_sample_rate,
        channel_layout=audio_channel_layout,
        output_path=f'{getattr(rx_host_b.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH)}/test_{audio_files[audio_type]["filename"]}_rx_b_{audio_files[audio_type]["channels"]}ch_{audio_files[audio_type]["sample_rate"]}Hz.{audio_format["ffmpeg_f"]}',
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
