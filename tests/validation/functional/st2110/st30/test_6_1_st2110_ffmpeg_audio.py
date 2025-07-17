# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import os
import pytest
from time import sleep


from common.ffmpeg_handler import ffmpeg_enums
from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmST2110AudioRx
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt30pTx
from Engine.const import MCM_ESTABLISH_TIMEOUT, MTL_ESTABLISH_TIMEOUT
from Engine.media_files import audio_files_25_03 as audio_files
from common.nicctl import Nicctl


@pytest.mark.usefixtures("media_proxy")
@pytest.mark.parametrize("audio_file", audio_files)
def test_6_1_st2110_ffmpeg_audio(hosts, test_config, audio_file):
    audio_file = audio_files[audio_file]

    tx_host = hosts["mesh-agent"]
    rx_a_host = hosts["mesh-agent"]
    rx_b_host = hosts["client"]

    try:
        audio_format = ffmpeg_enums.audio_file_format_to_format_dict(str(audio_file["format"]))
    except:
        pytest.skip(f"Unsupported audio format: {audio_file['format']}")

    audio_sample_rate = int(audio_file["sample_rate"])

    if audio_sample_rate not in [ar.value for ar in ffmpeg_enums.FFmpegAudioRate]:
        raise Exception(f"Not expected audio sample rate of {audio_file['sample_rate']}!")

    # Host A --- MTL FFmpeg Tx

    tx_input_path = tx_host.topology.extra_info.input_path
    tx_prefix_variables = dict(test_config["tx"]["prefix_variables"])
    tx_mtl_ffmpeg_path = tx_host.topology.extra_info.mtl_ffmpeg_path
    tx_mtl_ffmpeg_lib_path = tx_host.topology.extra_info.mtl_ffmpeg_lib_path
    tx_mtl_path = tx_host.topology.extra_info.mtl_path

    tx_prefix_variables["LD_LIBRARY_PATH"] = tx_mtl_ffmpeg_lib_path

    tx_nicctl = Nicctl(
        mtl_path=tx_mtl_path,
        host=tx_host,
    )
    tx_vfs = tx_nicctl.prepare_vfs_for_test(
        nic=tx_host.network_interfaces[0],
    )

    mtl_tx_input = FFmpegAudioIO(
        ar=int(audio_file["sample_rate"]),
        f=audio_format["ffmpeg_f"],
        ac=int(audio_file["channels"]),
        stream_loop=-1,
        input_path=os.path.join(tx_input_path, audio_file["filename"]),
    )
    mtl_tx_output = FFmpegMtlSt30pTx(
        f=audio_format["mtl_f"],
        p_tx_ip=test_config["broadcast_ip"],
        p_port=tx_vfs[0],
        p_sip=test_config["tx"]["p_sip"],
        udp_port=test_config["port"],
        payload_type=test_config["payload_type"],
        rx_queues=None,
        tx_queues=None,
        output_path="-",
    )
    mtl_tx_ffmpeg = FFmpeg(
        prefix_variables=tx_prefix_variables,
        ffmpeg_path=tx_mtl_ffmpeg_path,
        ffmpeg_input=mtl_tx_input,
        ffmpeg_output=mtl_tx_output,
    )
    mtl_tx_ffmpeg_executor = FFmpegExecutor(
        host=tx_host,
        ffmpeg_instance=mtl_tx_ffmpeg,
    )

    # Host A --- MCM FFmpeg Rx

    rx_a_output_path = rx_a_host.topology.extra_info.output_path
    rx_a_prefix_variables = dict(test_config["rx"]["prefix_variables"])
    rx_a_mcm_ffmpeg_path = rx_a_host.topology.extra_info.mcm_ffmpeg_path
    rx_a_mcm_ffmpeg_lib_path = rx_a_host.topology.extra_info.mcm_ffmpeg_lib_path

    rx_a_prefix_variables["LD_LIBRARY_PATH"] = rx_a_mcm_ffmpeg_lib_path

    mcm_rx_a_input = FFmpegMcmST2110AudioRx(
        ip_addr=test_config["broadcast_ip"],
        payload_type=test_config["payload_type"],
        channels=int(audio_file["channels"]),
        sample_rate=audio_sample_rate,
        f=audio_format["mcm_f"],
        conn_type=ffmpeg_enums.McmConnectionType.st.value,
        port=test_config["port"],
        input_path="-",
    )
    mcm_rx_a_output = FFmpegAudioIO(
        ar=int(audio_file["sample_rate"]),
        f=audio_format["ffmpeg_f"],
        ac=int(audio_file["channels"]),
        output_path=os.path.join(rx_a_output_path, audio_file["filename"]),
    )
    mcm_rx_a_ffmpeg = FFmpeg(
        prefix_variables=rx_a_prefix_variables,
        ffmpeg_path=rx_a_mcm_ffmpeg_path,
        ffmpeg_input=mcm_rx_a_input,
        ffmpeg_output=mcm_rx_a_output,
        yes_overwrite=True,
    )
    mcm_rx_a_ffmpeg_executor = FFmpegExecutor(
        host=rx_a_host,
        ffmpeg_instance=mcm_rx_a_ffmpeg,
    )

    # Host B --- MCM FFmpeg Rx

    rx_b_output_path = rx_b_host.topology.extra_info.output_path
    rx_b_prefix_variables = dict(test_config["rx"]["prefix_variables"])
    rx_b_mcm_ffmpeg_path = rx_b_host.topology.extra_info.mcm_ffmpeg_path
    rx_b_mcm_ffmpeg_lib_path = rx_b_host.topology.extra_info.mcm_ffmpeg_lib_path

    rx_b_prefix_variables["LD_LIBRARY_PATH"] = rx_b_mcm_ffmpeg_lib_path

    mcm_rx_b_input = FFmpegMcmST2110AudioRx(
        ip_addr=test_config["broadcast_ip"],
        payload_type=test_config["payload_type"],
        channels=int(audio_file["channels"]),
        sample_rate=audio_sample_rate,
        f=audio_format["mcm_f"],
        conn_type=ffmpeg_enums.McmConnectionType.st.value,
        port=test_config["port"],
        input_path="-",
    )
    mcm_rx_b_output = FFmpegAudioIO(
        ar=int(audio_file["sample_rate"]),
        f=audio_format["ffmpeg_f"],
        ac=int(audio_file["channels"]),
        output_path=os.path.join(rx_b_output_path, audio_file["filename"]),
    )
    mcm_rx_b_ffmpeg = FFmpeg(
        prefix_variables=rx_b_prefix_variables,
        ffmpeg_path=rx_b_mcm_ffmpeg_path,
        ffmpeg_input=mcm_rx_b_input,
        ffmpeg_output=mcm_rx_b_output,
        yes_overwrite=True,
    )
    mcm_rx_b_ffmpeg_executor = FFmpegExecutor(
        host=rx_b_host,
        ffmpeg_instance=mcm_rx_b_ffmpeg,
    )

    mtl_tx_ffmpeg_executor.start()
    sleep(MTL_ESTABLISH_TIMEOUT)

    mcm_rx_a_ffmpeg_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    mcm_rx_b_ffmpeg_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    sleep(test_config["test_time"])

    mtl_tx_ffmpeg_executor.stop()
    mcm_rx_a_ffmpeg_executor.stop()
    mcm_rx_b_ffmpeg_executor.stop()
