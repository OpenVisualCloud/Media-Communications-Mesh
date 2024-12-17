# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

import logging
import signal
import os
import Engine.client_json
import Engine.connection
import Engine.connection_json
import Engine.execute
import Engine.payload

def create_client_json(build, client):
    logging.debug("Client JSON:")
    for line in client.to_json().splitlines():
        logging.debug(line)
    output_path = os.path.join(build, "client.json")
    logging.debug(f"Client JSON path: {output_path}")
    client.prepare_and_save_json(output_path=output_path)

def create_connection_json(build, connection):
    logging.debug("Connection JSON:")
    for line in connection.to_json().splitlines():
        logging.debug(line)
    output_path = os.path.join(build, "connection.json")
    logging.debug(f"Connection JSON path: {output_path}")
    connection.prepare_and_save_json(output_path=output_path)

def run_rx_app(cwd, timeout=0):
    return Engine.execute.call("./RxApp", cwd=cwd, timeout=timeout)

def run_tx_app(file_path, rx_pid, cwd, testcmd=True):
    return Engine.execute.run(f"./TxApp {file_path} {rx_pid}", testcmd=testcmd, cwd=cwd)

def handle_tx_failure(tx):
    if tx.returncode != 0:
        Engine.execute.log_fail(f"TxApp failed with return code {tx.returncode}")

def stop_rx_app(rx):
    rx.process.send_signal(signal.SIGINT)
    rx.process.wait()

def run_rx_tx_with_file(file_path, build):
    cwd = build
    logging.debug(f"Constructed cwd path: {cwd}")

    # Check if the TxApp executable exists
    tx_app_path = os.path.join(cwd, "TxApp")
    if not os.path.isfile(tx_app_path):
        raise FileNotFoundError(f"TxApp executable not found at {tx_app_path}")

    try:
        rx = run_rx_app(cwd=cwd)
        tx = run_tx_app(file_path=file_path, rx_pid=rx.process.pid, cwd=cwd)
        handle_tx_failure(tx)
    finally:
        stop_rx_app(rx)