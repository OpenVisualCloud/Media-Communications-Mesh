# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

# 1.1 Scenario: MCM to MCM - Memif (single-node) - Ffmpeg audio
# https://jira.devtools.intel.com/browse/SDBQ-2243

import pytest
import logging

from ....Engine.media_files import audio_files

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    audio_file_format_to_format_dict,
    audio_channel_number_to_layout,
)

from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmMemifAudioIO


logger = logging.getLogger(__name__)


@pytest.mark.parametrize("audio_type", [k for k in audio_files.keys()])
def test_local_ffmpeg_audio(media_proxy, hosts, test_config, audio_type: str) -> None:
    # media_proxy fixture used only to ensure that the media proxy is running
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 1:
        pytest.skip("Local tests require at least 1 host")
    tx_host = rx_host = host_list[0]
    tx_prefix_variables = test_config["tx"].get("prefix_variables", None)
    rx_prefix_variables = test_config["rx"].get("prefix_variables", None)
    tx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = (
        tx_host.topology.extra_info.media_proxy["sdk_port"]
    )
    rx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = (
        rx_host.topology.extra_info.media_proxy["sdk_port"]
    )

    audio_format = audio_file_format_to_format_dict(
        str(audio_files[audio_type]["format"])
    )  # audio format
    audio_channel_layout = audio_files[audio_type].get(
        "channel_layout",
        audio_channel_number_to_layout(int(audio_files[audio_type]["channels"])),
    )

    if audio_files[audio_type]["sample_rate"] not in [48000, 44100, 96000]:
        raise Exception(
            f"Not expected audio sample rate of {audio_files[audio_type]['sample_rate']}!"
        )

    # >>>>> MCM Tx
    mcm_tx_inp = FFmpegAudioIO(
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        ar=int(audio_files[audio_type]["sample_rate"]),
        stream_loop = False,
        input_path = f'{test_config["tx"]["filepath"]}{audio_files[audio_type]["filename"]}'
    )
    mcm_tx_outp = FFmpegMcmMemifAudioIO(
        channels = int(audio_files[audio_type]["channels"]),
        sample_rate = int(audio_files[audio_type]["sample_rate"]),
        f = audio_format["mcm_f"],
        output_path = "-",
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables = tx_prefix_variables,
        ffmpeg_path = test_config["tx"]["ffmpeg_path"],
        ffmpeg_input = mcm_tx_inp,
        ffmpeg_output = mcm_tx_outp,
        yes_overwrite = False,
    )

    logger.debug(f"Tx command: {mcm_tx_ff.get_command()}")
    mcm_tx_executor = FFmpegExecutor(tx_host, ffmpeg_instance = mcm_tx_ff)

    # >>>>> MCM Rx
    mcm_rx_inp = FFmpegMcmMemifAudioIO(
        channels = int(audio_files[audio_type]["channels"]),
        sample_rate = int(audio_files[audio_type]['sample_rate']),
        f = audio_format["mcm_f"],
        input_path = "-",
    )
    mcm_rx_outp = FFmpegAudioIO(
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        ar=int(audio_files[audio_type]["sample_rate"]),
        channel_layout=audio_channel_layout,
        output_path=f'{test_config["rx"]["filepath"]}test_{audio_files[audio_type]["filename"]}',
    )
    mcm_rx_ff = FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=test_config["rx"]["ffmpeg_path"],
        ffmpeg_input=mcm_rx_inp,
        ffmpeg_output=mcm_rx_outp,
        yes_overwrite=True,
    )

    logger.debug(f"Rx command: {mcm_rx_ff.get_command()}")
    mcm_rx_executor = FFmpegExecutor(rx_host, ffmpeg_instance=mcm_rx_ff)

    mcm_rx_executor.start()
    mcm_tx_executor.start()
    mcm_rx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    mcm_tx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
