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
    FFmpegAudioRate,
    PacketTime,
    audio_file_format_to_format_dict,
)
from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt30pTx
from common.nicctl import Nicctl
from Engine.const import (
    DEFAULT_REMOTE_IP_ADDR,
    DEFAULT_REMOTE_PORT,
    DEFAULT_PAYLOAD_TYPE_ST2110_30,
    MCM_RXTXAPP_RUN_TIMEOUT,
    MCM_ESTABLISH_TIMEOUT,
    MTL_ESTABLISH_TIMEOUT,
    DEFAULT_PACING,
)
from Engine.mcm_apps import get_mtl_path
from Engine.media_files import audio_files_25_03 as audio_files

logger = logging.getLogger(__name__)


@pytest.mark.parametrize(
    "audio_type", [k for k in audio_files.keys() if "PCM8" not in k]
)
def test_st2110_rttxapp_mtl_to_mcm_audio(
    build_TestApp, hosts, media_proxy, media_path, test_config, audio_type, log_path
) -> None:
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    tx_host = host_list[0]
    rx_host = host_list[0]

    tx_prefix_variables = test_config["tx"].get("prefix_variables", {})
    tx_mtl_path = get_mtl_path(tx_host)

    audio_format = audio_file_format_to_format_dict(
        str(audio_files[audio_type]["format"])
    )

    audio_sample_rate = int(audio_files[audio_type]["sample_rate"])
    if audio_sample_rate not in [ar.value for ar in FFmpegAudioRate]:
        raise Exception(
            f"Not expected audio sample rate of {audio_files[audio_type]['sample_rate']}!"
        )

    tx_nicctl = Nicctl(
        host=tx_host,
        mtl_path=tx_mtl_path,
    )
    tx_vfs = tx_nicctl.prepare_vfs_for_test(tx_host.network_interfaces[0])
    mtl_tx_inp = FFmpegAudioIO(
        ar=audio_sample_rate,
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        stream_loop=-1,
        input_path=f'{test_config["tx"]["filepath"]}{audio_files[audio_type]["filename"]}',
    )
    mtl_tx_outp = FFmpegMtlSt30pTx(
        ptime=PacketTime.pt_1ms.value,
        p_port=str(tx_vfs[0]),
        p_sip=test_config["tx"]["p_sip"],
        p_tx_ip=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
        udp_port=test_config.get("port", DEFAULT_REMOTE_PORT),
        payload_type=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_30),
        pcm_fmt=audio_format["mtl_pcm_fmt"],
        channels=audio_files[audio_type]["channels"],
        sample_rate=audio_files[audio_type]["sample_rate"],
        f=audio_format["mtl_f"],
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

    rx_connection = Engine.rx_tx_app_connection.St2110_30(
        remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
        remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
        pacing=test_config.get("pacing", DEFAULT_PACING),
        payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_30),
    )

    rx_connection = Engine.rx_tx_app_connection.St2110_30(
        remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
        remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
        pacing=test_config.get("pacing", DEFAULT_PACING),
        payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_30),
    )

    rx_executor = utils.LapkaExecutor.Rx(
        host=rx_host,
        media_path=media_path,
        rx_tx_app_connection=rx_connection,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files[audio_type],
        file=audio_type,
        log_path=log_path,
    )

    rx_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    mtl_tx_executor.start()

    time.sleep(MTL_ESTABLISH_TIMEOUT)

    try:
        if rx_executor.process.running:
            rx_executor.process.wait(timeout=MCM_RXTXAPP_RUN_TIMEOUT)
    except Exception as e:
        logger.warning(f"RX executor did not finish in time or error occurred: {e}")

    rx_executor.stop()
    mtl_tx_executor.stop(wait=test_config.get("test_time_sec", 0.0))

    assert rx_executor.is_pass, "Receiver validation failed. Check logs for details."
