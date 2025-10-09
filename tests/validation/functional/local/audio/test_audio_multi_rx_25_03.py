# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
from time import sleep

import pytest

import Engine.rx_tx_app_connection
import Engine.rx_tx_app_engine_mcm as utils
import Engine.rx_tx_app_payload
from common.log_validation_utils import write_executor_validation_summary
from Engine.const import (
    DEFAULT_LOOP_COUNT,
    MCM_ESTABLISH_TIMEOUT,
    MCM_RXTXAPP_RUN_TIMEOUT,
)
from Engine.media_files import audio_files_25_03


@pytest.mark.parametrize("file", audio_files_25_03.keys())
def test_audio_multi_rx_25_03(
    build_TestApp, hosts, media_proxy, media_path, file, log_path
) -> None:

    # Get TX and RX hosts
    host_list = list(hosts.values())
    if len(host_list) < 1:
        pytest.skip("Local tests require at least 1 host")
    tx_host = host_list[0]

    # Using the same host for all RX instances in local test
    rx_host1 = rx_host2 = rx_host3 = host_list[0]

    tx_executor = utils.LapkaExecutor.Tx(
        host=tx_host,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.MultipointGroup,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files_25_03[file],
        file=file,
        log_path=log_path,
        loop=DEFAULT_LOOP_COUNT,
        instance_num=1,  # Add instance number for unique log file
    )

    # Create 3 RX executors
    # Create RX executors with instance numbers
    rx_executor1 = utils.LapkaExecutor.Rx(
        host=rx_host1,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.MultipointGroup,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files_25_03[file],
        file=f"{file}_rx1",
        log_path=log_path,
        instance_num=1,  # Add instance number for unique log file
    )

    rx_executor2 = utils.LapkaExecutor.Rx(
        host=rx_host2,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.MultipointGroup,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files_25_03[file],
        file=f"{file}_rx2",
        log_path=log_path,
        instance_num=2,  # Add instance number for unique log file
    )

    rx_executor3 = utils.LapkaExecutor.Rx(
        host=rx_host3,
        media_path=media_path,
        rx_tx_app_connection=Engine.rx_tx_app_connection.MultipointGroup,
        payload_type=Engine.rx_tx_app_payload.Audio,
        file_dict=audio_files_25_03[file],
        file=f"{file}_rx3",
        log_path=log_path,
        instance_num=3,  # Add instance number for unique log file
    )

    rx_executor1.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    rx_executor2.start()
    sleep(MCM_ESTABLISH_TIMEOUT)
    rx_executor3.start()

    sleep(MCM_ESTABLISH_TIMEOUT)

    tx_executor.start()

    rx_executors = [rx_executor1, rx_executor2, rx_executor3]
    for i, rx_executor in enumerate(rx_executors, 1):
        try:
            if rx_executor.process.running:
                logging.info(
                    f"Waiting up to {MCM_RXTXAPP_RUN_TIMEOUT}s for RX executor {i} to complete"
                )
                rx_executor.process.wait(timeout=MCM_RXTXAPP_RUN_TIMEOUT)
        except Exception as e:
            logging.warning(
                f"RX executor {i} did not finish in time or error occurred: {e}"
            )

    tx_executor.stop()

    for i, rx_executor in enumerate(rx_executors, 1):
        logging.info(f"Stopping RX executor {i}")
        rx_executor.stop()

    # TODO add validate() function to check if the output files are correct

    # Cleanup all RX executors
    for rx_executor in rx_executors:
        rx_executor.cleanup()

    # Write the consolidated validation summary with the simplified API
    write_executor_validation_summary(log_path, tx_executor, rx_executors)

    # Verify all processes passed
    assert tx_executor.is_pass is True, "TX process did not pass"
    for i, rx_executor in enumerate(rx_executors, 1):
        assert rx_executor.is_pass is True, f"RX process {i} did not pass"
