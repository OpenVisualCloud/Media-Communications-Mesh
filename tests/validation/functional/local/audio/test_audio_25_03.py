# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
from time import sleep

import pytest

import Engine.rx_tx_app_connection
import Engine.rx_tx_app_engine_mcm as utils
import Engine.rx_tx_app_payload
from Engine.const import (
    DEFAULT_LOOP_COUNT,
    MCM_ESTABLISH_TIMEOUT,
    MCM_RXTXAPP_RUN_TIMEOUT,
)
from Engine.media_files import audio_files_25_03
from common.log_validation_utils import write_executor_validation_summary


@pytest.mark.parametrize(
    "file",
    [
        pytest.param("PCM16_48000_Mono", marks=pytest.mark.smoke),
        *[f for f in audio_files_25_03.keys() if f != "PCM16_48000_Stereo"],
    ],
)
def test_audio_25_03(
    build_TestApp, hosts, media_proxy, media_path, file, log_path
) -> None:

    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 1:
        pytest.skip("Local tests require at least 1 host")
    tx_host = rx_host = host_list[0]

    tx_executor = utils.LapkaExecutor.Tx(
        host=tx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.MultipointGroup,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files_25_03[file],
        file=file,
        log_path=log_path,
        loop=DEFAULT_LOOP_COUNT,
    )
    rx_executor = utils.LapkaExecutor.Rx(
        host=rx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.MultipointGroup,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files_25_03[file],
        file=file,
        log_path=log_path,
    )

    rx_executor.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    tx_executor.start()

    try:
        if rx_executor.process.running:
            logging.info(
                f"Waiting up to {MCM_RXTXAPP_RUN_TIMEOUT}s for RX process to complete"
            )
            rx_executor.process.wait(timeout=MCM_RXTXAPP_RUN_TIMEOUT)
    except Exception as e:
        logging.warning(f"RX executor did not finish in time or error occurred: {e}")

    tx_executor.stop()

    rx_executor.stop()

    # TODO add validate() function to check if the output file is correct

    rx_executor.cleanup()
    # Write the consolidated validation summary
    write_executor_validation_summary(log_path, tx_executor, rx_executor)

    assert tx_executor.is_pass is True, "TX process did not pass"
    assert rx_executor.is_pass is True, "RX process did not pass"
