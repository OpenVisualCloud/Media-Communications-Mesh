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
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt30pRx
from common.nicctl import Nicctl
from Engine.const import (
    DEFAULT_LOOP_COUNT,
    DEFAULT_REMOTE_IP_ADDR,
    DEFAULT_REMOTE_PORT,
    DEFAULT_PACING,
    DEFAULT_PAYLOAD_TYPE_ST2110_30,
    MCM_ESTABLISH_TIMEOUT,
    MTL_ESTABLISH_TIMEOUT,
    MCM_RXTXAPP_RUN_TIMEOUT,
)
from Engine.mcm_apps import get_mtl_path
from Engine.media_files import audio_files_25_03 as audio_files

logger=logging.getLogger(__name__)

@pytest.mark.parametrize(
    "audio_type", [k for k in audio_files.keys() if "PCM8" not in k]
)
def test_st2110_rttxapp_mcm_to_mtl_audio(
    build_TestApp, hosts, media_proxy, media_path, test_config, audio_type, log_path
) -> None:
    # Get TX and RX hosts
    host_list=list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    tx_host=host_list[0]
    rx_host_a=host_list[0]
    rx_host_b=host_list[1]
    rx_mtl_host=host_list[0]

    tx_executor=utils.LapkaExecutor.Tx(
        host=tx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.St2110_30(
            remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
            remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
            pacing=test_config.get("pacing", DEFAULT_PACING),
            payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_30),
        ),
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files[audio_type],
        file=audio_type,
        log_path=log_path,
        loop=DEFAULT_LOOP_COUNT,
    )

    rx_prefix_variables=test_config["rx"].get("prefix_variables", {})
    rx_mtl_path=get_mtl_path(rx_mtl_host)

    audio_format=audio_file_format_to_format_dict(
        str(audio_files[audio_type]["format"])
    )

    audio_sample_rate=int(audio_files[audio_type]["sample_rate"])
    if audio_sample_rate not in [ar.value for ar in FFmpegAudioRate]:
        raise Exception(
            f"Not expected audio sample rate of {audio_files[audio_type]['sample_rate']}!"
        )

    rx_nicctl=Nicctl(
        mtl_path=rx_mtl_path,
        host=rx_mtl_host,
    )
    rx_vfs=rx_nicctl.prepare_vfs_for_test(rx_mtl_host.network_interfaces[0])
    
    mtl_rx_inp=FFmpegMtlSt30pRx(
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
    mtl_rx_outp=FFmpegAudioIO(
        ar=audio_sample_rate,
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        output_path=f'{test_config["rx"]["filepath"]}test_{audio_files[audio_type]["filename"]}_{audio_files[audio_type]["channels"]}ch_{audio_files[audio_type]["sample_rate"]}Hz.{audio_format["ffmpeg_f"]}',
    )
    mtl_rx_ff=FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=test_config["rx"]["ffmpeg_path"],
        ffmpeg_input=mtl_rx_inp,
        ffmpeg_output=mtl_rx_outp,
        yes_overwrite=True,
    )
    logger.debug(
        f"Mtl rx command executed on {rx_mtl_host.name}: {mtl_rx_ff.get_command()}"
    )
    mtl_rx_executor=FFmpegExecutor(
        host=rx_mtl_host, ffmpeg_instance=mtl_rx_ff, log_path=log_path
    )

    rx_executor_a=utils.LapkaExecutor.Rx(
        host=rx_host_a,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.St2110_30(
            remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
            remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
            pacing=test_config.get("pacing", DEFAULT_PACING),
            payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_30),
        ),
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files[audio_type],
        file=audio_type,
        log_path=log_path,
    )

    rx_executor_b=utils.LapkaExecutor.Rx(
        host=rx_host_b,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.St2110_30(
            remoteIpAddr=test_config.get("broadcast_ip", DEFAULT_REMOTE_IP_ADDR),
            remotePort=test_config.get("port", DEFAULT_REMOTE_PORT),
            pacing=test_config.get("pacing", DEFAULT_PACING),
            payloadType=test_config.get("payload_type", DEFAULT_PAYLOAD_TYPE_ST2110_30),
        ),
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files[audio_type],
        file=audio_type,
        log_path=log_path,
    )

    tx_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    rx_executor_b.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    rx_executor_a.start()
    sleep(MCM_ESTABLISH_TIMEOUT)

    mtl_rx_executor.start()

    try:
        if rx_executor_a.process.running:
            rx_executor_a.process.wait(timeout=MCM_RXTXAPP_RUN_TIMEOUT)
    except Exception as e:
        logger.warning(f"RX executor did not finish in time or error occurred: {e}")

    rx_executor_a.stop()
    rx_executor_b.stop()
    mtl_rx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    tx_executor.stop()

    assert (
        rx_executor_a.is_pass
    ), "Receiver A validation failed. Check logs for details."
    assert (
        rx_executor_b.is_pass
    ), "Receiver B validation failed. Check logs for details."
