# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

# | 20 | FFmpeg | PCM 16-bit Big-Endian (pcm_s16be) | 48000 Hz | 1 ms | 2 (S - Stereo) |

import logging
import time
import pytest

from common.ffmpeg_handler.ffmpeg import FFmpeg, FFmpegExecutor
from common.ffmpeg_handler.ffmpeg_enums import (
    FFmpegAudioRate, McmConnectionType, PacketTime,
    audio_file_format_to_format_dict, audio_channel_number_to_layout)
from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmST2110AudioRx
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt30pTx

from common.nicctl import Nicctl

from Engine.mcm_apps import get_media_proxy_port, get_mtl_path

from ....Engine.media_files import audio_files

logger = logging.getLogger(__name__)

MAX_TEST_TIME_DEFAULT = 60  # seconds
EARLY_STOP_THRESHOLD_PERCENTAGE = 20  # percentage of max_test_time to consider an early stop

@pytest.mark.parametrize("audio_type", [k for k in audio_files.keys()])
def test_st2110_ffmpeg_mtl_to_mcm_audio(media_proxy, hosts, test_config, audio_type: str) -> None:
    # media_proxy fixture used only to ensure that the media proxy is running
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    tx_host = host_list[0]
    rx_host = host_list[1]
    tx_prefix_variables = test_config["tx"].get("prefix_variables", {})
    rx_prefix_variables = test_config["rx"].get("prefix_variables", {})
    rx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = get_media_proxy_port(rx_host)
    tx_mtl_path = get_mtl_path(tx_host)
    rx_mtl_path = get_mtl_path(rx_host)

    audio_format = audio_file_format_to_format_dict(str(audio_files[audio_type]["format"])) # audio format
    audio_channel_layout = audio_files[audio_type].get(
        "channel_layout",
        audio_channel_number_to_layout(int(audio_files[audio_type]["channels"]))
    )

    # Set audio sample rate based on the audio file sample rate
    audio_sample_rate = int(audio_files[audio_type]['sample_rate'])
    if audio_sample_rate not in [ar.value for ar in FFmpegAudioRate]:
        raise Exception(f"Not expected audio sample rate of {audio_files[audio_type]['sample_rate']}!")

    # Prepare Tx VFs
    tx_nicctl = Nicctl(
        host = tx_host,
        mtl_path = tx_mtl_path,
    )
    tx_vfs = tx_nicctl.prepare_vfs_for_test(tx_host.network_interfaces[0])
    if not tx_vfs:
        tx_pf = str(tx_host.network_interfaces[0].pci_address)

    # Prepare Rx VFs
    rx_nicctl = Nicctl(
        host = rx_host,
        mtl_path = rx_mtl_path,
    )
    _ = rx_nicctl.prepare_vfs_for_test(rx_host.network_interfaces[0])

    # >>>>> MTL Tx
    mtl_tx_inp = FFmpegAudioIO(
        ar = audio_sample_rate,
        f = audio_format["ffmpeg_f"],
        ac = int(audio_files[audio_type]["channels"]),
        stream_loop = -1,
        input_path = f'{test_config["tx"]["filepath"]}{audio_files[audio_type]["filename"]}',
    )
    mtl_tx_outp = FFmpegMtlSt30pTx(
        ptime = PacketTime.pt_1ms.value,
        p_port = str(tx_vfs[0] if tx_vfs else tx_pf), # use VF or PF if no VFs are available
        p_sip = test_config["tx"]["p_sip"],
        p_tx_ip = test_config["broadcast_ip"],
        udp_port = test_config["port"],
        payload_type = test_config["payload_type"],
        pcm_fmt = audio_format["mtl_pcm_fmt"],
        channels = audio_files[audio_type]["channels"],
        sample_rate = audio_files[audio_type]["sample_rate"],
        f = audio_format["mtl_f"],
        output_path = "-",
        rx_queues = None,
        tx_queues = None,
    )
    mtl_tx_ff = FFmpeg(
        prefix_variables = tx_prefix_variables,
        ffmpeg_path = test_config["tx"]["ffmpeg_path"],
        ffmpeg_input = mtl_tx_inp,
        ffmpeg_output = mtl_tx_outp,
        yes_overwrite = False,
    )
    logger.debug(f"Tx command executed on {tx_host.name}: {mtl_tx_ff.get_command()}")
    mtl_tx_executor = FFmpegExecutor(tx_host, ffmpeg_instance=mtl_tx_ff)

    # >>>>> MCM Rx
    mcm_rx_inp = FFmpegMcmST2110AudioRx(
        ip_addr = test_config["broadcast_ip"],
        payload_type = test_config["payload_type"],
        channels = audio_files[audio_type]["channels"],
        sample_rate = audio_sample_rate,
        ptime = PacketTime.pt_1ms.value,
        f = audio_format["mcm_f"],
        conn_type = McmConnectionType.st.value,
        port = test_config["port"],
        input_path = "-",
    )
    mcm_rx_outp = FFmpegAudioIO(
        f = audio_format["ffmpeg_f"],
        ac = int(audio_files[audio_type]["channels"]),
        ar = int(audio_files[audio_type]["sample_rate"]),
        channel_layout = audio_channel_layout,
        output_path = f'{test_config["rx"]["filepath"]}test_{audio_files[audio_type]["filename"]}',
    )

    mcm_rx_ff = FFmpeg(
        prefix_variables = rx_prefix_variables,
        ffmpeg_path = test_config["rx"]["ffmpeg_path"],
        ffmpeg_input = mcm_rx_inp,
        ffmpeg_output = mcm_rx_outp,
        yes_overwrite = True,
    )
    logger.debug(f"Rx command executed on {rx_host.name}: {mcm_rx_ff.get_command()}")
    mcm_rx_executor = FFmpegExecutor(rx_host, ffmpeg_instance = mcm_rx_ff)

    time.sleep(2) # wait for media_proxy to start
    mcm_rx_executor.start()
    mtl_tx_executor.start()

    max_test_time = test_config.get("test_time_sec", MAX_TEST_TIME_DEFAULT)
    tx_stopping_time = mtl_tx_executor.stop(wait = max_test_time)
    logger.info(f"Tx stopping time: ~{tx_stopping_time} seconds")

    # if Tx stops in EARLY_STOP_THRESHOLD_PERCENTAGE% of the max_test_time, it is considered an early stop
    if tx_stopping_time <= (max_test_time * EARLY_STOP_THRESHOLD_PERCENTAGE/100):
        logger.warning(f"It seems FFmpeg on {tx_host.name} stopped too early! Cutting down the Rx stop wait time to {tx_stopping_time} seconds.")
        rx_stopping_wait = tx_stopping_time
    else:
        rx_stopping_wait = max_test_time - tx_stopping_time

    rx_stopping_time = mcm_rx_executor.stop(wait = rx_stopping_wait)
    logger.info(f"Rx stopping time: ~{rx_stopping_time} seconds")
