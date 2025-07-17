# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import logging
import pytest
from time import sleep

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    audio_channel_number_to_layout,
    audio_file_format_to_format_dict,
    FFmpegAudioRate,
    PacketTime,
)
from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt30pRx
from common.nicctl import Nicctl
from Engine.const import *
from Engine.media_files import audio_files_25_03 as audio_files
from Engine import rx_tx_app_connection, rx_tx_app_engine_mcm, rx_tx_app_payload

logger = logging.getLogger(__name__)


@pytest.mark.usefixtures("media_proxy")
@pytest.mark.parametrize("audio_type", list(audio_files.keys()))
def test_3_2_st2110_standalone_audio(hosts, test_config, audio_type, log_path):
    try:
        audio_format = audio_file_format_to_format_dict(str(audio_files[audio_type]["format"]))
    except:
        pytest.skip(f"Unsupported audio format: {audio_files[audio_type]['format']}")

    audio_channel_layout = audio_files[audio_type].get(
        "channel_layout", audio_channel_number_to_layout(int(audio_files[audio_type]["channels"]))
    )

    audio_sample_rate = int(audio_files[audio_type]["sample_rate"])
    if audio_sample_rate not in [ar.value for ar in FFmpegAudioRate]:
        raise Exception(f"Not expected audio sample rate of {audio_files[audio_type]['sample_rate']}!")

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
        rx_tx_app_connection=lambda: rx_tx_app_connection.St2110_30(
            remoteIpAddr=test_config.get("broadcast_ip"),
            remotePort=test_config.get("port"),
            pacing=test_config.get("pacing"),
            payloadType=test_config.get("payload_type"),
        ),
        payload_type=rx_tx_app_payload.Audio,
        file_dict=audio_files[audio_type],
        file=audio_type,
        log_path=log_path,
        loop=-1,
    )

    # Rx

    rx_ffmpeg_input = FFmpegMtlSt30pRx(
        fb_cnt=None,
        timeout_s=None,
        init_retry=None,
        sample_rate=audio_sample_rate,
        channels=int(audio_files[audio_type]["channels"]),
        pcm_fmt=audio_format["mtl_pcm_fmt"],
        ptime=PacketTime.pt_1ms.value,
        p_rx_ip=test_config["broadcast_ip"],
        udp_port=int(test_config["port"]),
        payload_type=int(test_config["payload_type"]),
        input_path="-",
        rx_queues=None,
        tx_queues=None,
        p_port=str(vfs[0]),
        p_sip=test_config["rx"]["p_sip"],
    )
    rx_ffmpeg_output = FFmpegAudioIO(
        ar=int(audio_files[audio_type]["sample_rate"]),
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        channel_layout=audio_channel_layout,
        output_path=f'{test_config["rx"]["filepath"]}test_{audio_files[audio_type]["filename"]}',
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
