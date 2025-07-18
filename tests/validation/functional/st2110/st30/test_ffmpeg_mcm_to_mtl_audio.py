# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
import time
import pytest

from common.ffmpeg_handler.ffmpeg import (
    FFmpeg,
    FFmpegExecutor,
    no_proxy_to_prefix_variables,
)
from common.ffmpeg_handler.ffmpeg_enums import (
    FFmpegAudioRate,
    McmConnectionType,
    PacketTime,
    audio_file_format_to_format_dict,
    audio_channel_number_to_layout,
)
from common.ffmpeg_handler.ffmpeg_io import FFmpegAudioIO
from common.ffmpeg_handler.mcm_ffmpeg import FFmpegMcmST2110AudioTx
from common.ffmpeg_handler.mtl_ffmpeg import FFmpegMtlSt30pRx

from ....Engine.media_files import audio_files

from common.nicctl import Nicctl

logger = logging.getLogger(__name__)


@pytest.mark.parametrize("audio_type", [k for k in audio_files.keys()])
def test_st2110_ffmpeg_mcm_to_mtl_audio(
    media_proxy, hosts, test_config, audio_type: str
) -> None:
    # media_proxy fixture used only to ensure that the media proxy is running
    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    # TX configuration
    tx_host = host_list[0]
    tx_prefix_variables = test_config["tx"].get("prefix_variables", {})
    tx_prefix_variables = no_proxy_to_prefix_variables(tx_host, tx_prefix_variables)
    tx_prefix_variables["MCM_MEDIA_PROXY_PORT"] = (
        tx_host.topology.extra_info.media_proxy.get("sdk_port")
    )

    # RX configuration
    rx_host = host_list[1]
    rx_prefix_variables = test_config["rx"].get("prefix_variables", {})
    rx_prefix_variables = no_proxy_to_prefix_variables(rx_host, rx_prefix_variables)
    rx_mtl_path = rx_host.topology.extra_info.mtl_path

    # Audio configuration
    audio_format = audio_file_format_to_format_dict(
        str(audio_files[audio_type]["format"])
    )  # audio format
    audio_channel_layout = audio_files[audio_type].get(
        "channel_layout",
        audio_channel_number_to_layout(int(audio_files[audio_type]["channels"])),
    )

    if int(audio_files[audio_type]["sample_rate"]) == 48000:
        audio_sample_rate = FFmpegAudioRate.k48.value
    elif int(audio_files[audio_type]["sample_rate"]) == 44100:
        audio_sample_rate = FFmpegAudioRate.k44.value
    elif int(audio_files[audio_type]["sample_rate"]) == 96000:
        audio_sample_rate = FFmpegAudioRate.k96.value
    else:
        pytest.skip(
            f"Skipping test due to unsupported audio sample rate: {audio_files[audio_type]['sample_rate']}"
        )

    # Prepare Rx VFs
    rx_nicctl = Nicctl(
        host=rx_host,
        mtl_path=rx_mtl_path,
    )
    rx_vfs = rx_nicctl.prepare_vfs_for_test(rx_host.network_interfaces[0])

    # MCM Tx
    mcm_tx_inp = FFmpegAudioIO(
        ar=audio_sample_rate,
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        # FFmpegIO:
        stream_loop=-1,
        input_path=f'{test_config["tx"]["filepath"]}{audio_files[audio_type]["filename"]}',
    )
    mcm_tx_outp = FFmpegMcmST2110AudioTx(  # TODO: Verify the variables
        ip_addr=test_config["broadcast_ip"],  # ? #FFmpegMcmST2110AudioTx
        # FFmpegMcmST2110AudioIO:
        payload_type=test_config["payload_type"],
        channels=int(audio_files[audio_type]["channels"]),
        sample_rate=int(audio_files[audio_type]["sample_rate"]),
        ptime=PacketTime.pt_1ms.value,
        f=audio_format["mcm_f"],
        # FFmpegMcmST2110CommonIO:
        conn_type=McmConnectionType.st.value,
        urn=None,  # ?!
        port=test_config["port"],
        output_path="-",  # FFmpegIO
    )
    mcm_tx_ff = FFmpeg(
        prefix_variables=tx_prefix_variables,
        ffmpeg_path=test_config["tx"]["ffmpeg_path"],
        ffmpeg_input=mcm_tx_inp,
        ffmpeg_output=mcm_tx_outp,
        yes_overwrite=False,
    )
    logger.debug(f"Tx command executed on {tx_host.name}: {mcm_tx_ff.get_command()}")
    mcm_tx_executor = FFmpegExecutor(tx_host, ffmpeg_instance=mcm_tx_ff)

    # MTL Rx
    mtl_rx_inp = FFmpegMtlSt30pRx(  # TODO: Verify the variables
        # FFmpegMtlSt30pRx:
        fb_cnt=None,
        timeout_s=None,
        init_retry=None,
        sample_rate=audio_sample_rate,
        channels=int(audio_files[audio_type]["channels"]),
        pcm_fmt=audio_format["mtl_pcm_fmt"],
        ptime=PacketTime.pt_1ms.value,
        p_rx_ip=test_config["broadcast_ip"],  # FFmpegMtlCommonRx
        # FFmpegMtlCommonIO:
        udp_port=int(test_config["port"]),
        payload_type=int(test_config["payload_type"]),
        # FFmpegIO:
        input_path="-",
        rx_queues=None,
        tx_queues=None,
        p_port=str(rx_vfs[0]),
        p_sip=test_config["rx"]["p_sip"],
    )
    mtl_rx_outp = FFmpegAudioIO(
        f=audio_format["ffmpeg_f"],
        ac=int(audio_files[audio_type]["channels"]),
        ar=int(audio_files[audio_type]["sample_rate"]),
        channel_layout=audio_channel_layout,  # ?!
        output_path=f'{test_config["rx"]["filepath"]}test_{audio_files[audio_type]["filename"]}',
    )

    mtl_rx_ff = FFmpeg(
        prefix_variables=rx_prefix_variables,
        ffmpeg_path=test_config["rx"]["ffmpeg_path"],
        ffmpeg_input=mtl_rx_inp,
        ffmpeg_output=mtl_rx_outp,
        yes_overwrite=True,
    )
    logger.debug(f"Rx command executed on {rx_host.name}: {mtl_rx_ff.get_command()}")
    mtl_rx_executor = FFmpegExecutor(rx_host, ffmpeg_instance=mtl_rx_ff)

    time.sleep(2)  # wait for media_proxy to start
    mtl_rx_executor.start()
    mcm_tx_executor.start()
    time.sleep(2)  # ensure executors are started
    mtl_rx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
    mcm_tx_executor.stop(wait=test_config.get("test_time_sec", 0.0))
