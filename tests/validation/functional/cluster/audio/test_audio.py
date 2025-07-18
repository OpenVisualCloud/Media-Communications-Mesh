# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
from time import sleep

import pytest

import Engine.rx_tx_app_connection
import Engine.rx_tx_app_engine_mcm as utils
import Engine.rx_tx_app_payload
from Engine.const import DEFAULT_LOOP_COUNT, MCM_ESTABLISH_TIMEOUT
from Engine.media_files import audio_files

logger = logging.getLogger(__name__)


@pytest.mark.parametrize("file", audio_files.keys())
def test_audio(build_TestApp, hosts, media_proxy, media_path, file, log_path) -> None:

    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 2:
        pytest.skip("Dual tests require at least 2 hosts")
    tx_host = host_list[0]
    rx_host = host_list[1]

    tx_executor = utils.LapkaExecutor.Tx(
        host=tx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.Rdma,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files[file],
        file=file,
        log_path=log_path,
        loop=DEFAULT_LOOP_COUNT,
    )
    rx_executor = utils.LapkaExecutor.Rx(
        host=rx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.Rdma,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files[file],
        file=file,
        log_path=log_path,
    )

    rx_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    tx_executor.start()

    try:
        if rx_executor.process.running:
            rx_executor.process.wait(timeout=MCM_ESTABLISH_TIMEOUT)
    except Exception as e:
        logger.warning(f"RX executor did not finish in time or error occurred: {e}")

    tx_executor.stop()
    rx_executor.stop()

    assert tx_executor.is_pass is True, "TX process did not pass"
    assert rx_executor.is_pass is True, "RX process did not pass"

    # TODO add validate() function to check if the output file is correct
