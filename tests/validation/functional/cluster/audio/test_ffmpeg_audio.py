# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import logging
import pytest
import time

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    audio_file_format_to_format_dict,
    audio_channel_number_to_layout,
)

from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmMultipointGroupAudioIO

from Engine.media_files import audio_files

logger = logging.getLogger(__name__)

MAX_WAIT_TIME = 180.0  # maximum wait time for the process to stop, in seconds


@pytest.mark.parametrize("audio_type", [file for file in audio_files.keys()])
def test_cluster_ffmpeg_audio(hosts, media_proxy, test_config, audio_type: str, log_path) -> None:
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    tx_host = host_list[0]
    rx_host = host_list[1]
    tx_prefix_variables = test_config["tx"].get("prefix_variables", {})
    rx_prefix_variables = test_config["rx"].get("prefix_variables", {})
    tx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = tx_host.topology.extra_info.media_proxy["sdk_port"]
    rx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = rx_host.topology.extra_info.media_proxy["sdk_port"]

    audio_format = audio_file_format_to_format_dict(str(audio_files[audio_type]["format"]))  # audio format
    audio_channel_layout = audio_files[audio_type].get(
        "channel_layout",
        audio_channel_number_to_layout(int(audio_files[audio_type]["channels"])),
    )

    if audio_files[audio_type]["sample_rate"] not in [48000, 44100, 96000]:
        raise Exception(f"Not expected audio sample rate of {audio_files[audio_type]['sample_rate']}!")

    # >>>>> MCM Tx
    mcm_tx_inp = FFmpegAudioIO(
        ar=int(audio_files[audio_type]["sample_rate"]),
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        stream_loop=False,
        input_path=f'{test_config["tx"]["filepath"]}{audio_files[audio_type]["filename"]}',
    )
    mcm_tx_outp = FFmpegMcmMultipointGroupAudioIO(
        channels=int(audio_files[audio_type]["channels"]),
        sample_rate=int(audio_files[audio_type]["sample_rate"]),
        f=audio_format["mcm_f"],
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
    mcm_rx_inp = FFmpegMcmMultipointGroupAudioIO(
        channels=int(audio_files[audio_type]["channels"]),
        sample_rate=int(audio_files[audio_type]["sample_rate"]),
        f=audio_format["mcm_f"],
        input_path="-",
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

    logger.debug(f"Rx command on {rx_host.name}: {mcm_rx_ff.get_command()}")
    mcm_rx_executor = FFmpegExecutor(rx_host, log_path=log_path, ffmpeg_instance=mcm_rx_ff)

    mcm_rx_executor.start()
    mcm_tx_executor.start()
    time.sleep(1)  # wait for the processes to start
    mcm_rx_executor.stop(wait=test_config.get("test_time_sec", MAX_WAIT_TIME))
    mcm_tx_executor.stop(wait=test_config.get("test_time_sec", MAX_WAIT_TIME))

    # TODO: check if the output file exists and is > 0 bytes
