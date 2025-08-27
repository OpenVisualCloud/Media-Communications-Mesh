# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

# 1.1 Scenario: MCM to MCM - Memif (single-node) - Ffmpeg audio

import pytest
import logging

from Engine.media_files import audio_files_25_03

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    audio_file_format_to_format_dict,
    audio_channel_number_to_layout,
)

from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmMemifAudioIO
from Engine.const import (
    FFMPEG_RUN_TIMEOUT,
    DEFAULT_OUTPUT_PATH,
)


logger = logging.getLogger(__name__)


@pytest.mark.usefixtures("media_proxy")
@pytest.mark.parametrize(
    "audio_type",
    [
        pytest.param("PCM16_48000_Stereo", marks=pytest.mark.smoke),
        *[f for f in audio_files_25_03.keys() if f != "PCM16_48000_Stereo"],
    ],
)
def test_local_ffmpeg_audio(
    hosts, test_config, audio_type: str, log_path, media_path
) -> None:
    host_list = list(hosts.values())
    if len(host_list) < 1:
        pytest.skip("Local tests require at least 1 host")
    tx_host = rx_host = host_list[0]

    if hasattr(tx_host.topology.extra_info, "mcm_prefix_variables"):
        prefix_variables = dict(tx_host.topology.extra_info.mcm_prefix_variables)
    else:
        prefix_variables = {}
    prefix_variables["MCM_MEDIA_PROXY_PORT"] = tx_host.topology.extra_info.media_proxy[
        "sdk_port"
    ]

    audio_format = audio_file_format_to_format_dict(
        str(audio_files_25_03[audio_type]["format"])
    )  # audio format
    audio_channel_layout = audio_files_25_03[audio_type].get(
        "channel_layout",
        audio_channel_number_to_layout(int(audio_files_25_03[audio_type]["channels"])),
    )

    if audio_files_25_03[audio_type]["sample_rate"] not in [48000, 44100, 96000]:
        raise Exception(
            f"Not expected audio sample rate of {audio_files_25_03[audio_type]['sample_rate']}!"
        )

    # >>>>> MCM Tx
    mcm_tx_inp = FFmpegAudioIO(
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files_25_03[audio_type]["channels"]),
        ar=int(audio_files_25_03[audio_type]["sample_rate"]),
        stream_loop=False,
        input_path=f'{media_path}{audio_files_25_03[audio_type]["filename"]}',
    )
    mcm_tx_outp = FFmpegMcmMemifAudioIO(
        channels=int(audio_files_25_03[audio_type]["channels"]),
        sample_rate=int(audio_files_25_03[audio_type]["sample_rate"]),
        f=audio_format["mcm_f"],
        output_path="-",
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables=prefix_variables,
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
    mcm_rx_inp = FFmpegMcmMemifAudioIO(
        channels=int(audio_files_25_03[audio_type]["channels"]),
        sample_rate=int(audio_files_25_03[audio_type]["sample_rate"]),
        f=audio_format["mcm_f"],
        input_path="-",
    )
    mcm_rx_outp = FFmpegAudioIO(
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files_25_03[audio_type]["channels"]),
        ar=int(audio_files_25_03[audio_type]["sample_rate"]),
        channel_layout=audio_channel_layout,
        output_path=f'{getattr(rx_host.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH)}/test_{audio_files_25_03[audio_type]["filename"]}',
    )
    mcm_rx_ff = FFmpeg(
        prefix_variables=prefix_variables,
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
        if mcm_rx_executor._processes[0].running:
            mcm_rx_executor._processes[0].wait(timeout=FFMPEG_RUN_TIMEOUT)
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
