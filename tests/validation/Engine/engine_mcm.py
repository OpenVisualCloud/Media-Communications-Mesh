# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

import logging
import os
import signal
import subprocess

import Engine.client_json
import Engine.connection
import Engine.connection_json
import Engine.execute
import Engine.payload


def create_client_json(build: str, client: Engine.client_json.ClientJson):
    logging.debug("Client JSON:")
    for line in client.to_json().splitlines():
        logging.debug(line)
    output_path = os.path.join(build, "tests", "tools", "TestApp", "build", "client.json")
    logging.debug(f"Client JSON path: {output_path}")
    client.prepare_and_save_json(output_path=output_path)


def create_connection_json(build: str, connection: Engine.connection_json.ConnectionJson):
    logging.debug("Connection JSON:")
    for line in connection.to_json().splitlines():
        logging.debug(line)
    output_path = os.path.join(build, "tests", "tools", "TestApp", "build", "connection.json")
    logging.debug(f"Connection JSON path: {output_path}")
    connection.prepare_and_save_json(output_path=output_path)


def run_rx_app(cwd: str, timeout: int = 0) -> Engine.execute.AsyncProcess:
    return Engine.execute.call("./RxApp", cwd=cwd, timeout=timeout)


def run_tx_app(file_path: str, rx_pid: int, cwd: str, testcmd: bool = True) -> subprocess.CompletedProcess:
    return Engine.execute.run(f"./TxApp {file_path} {rx_pid}", testcmd=testcmd, cwd=cwd)


def handle_tx_failure(tx: subprocess.CompletedProcess):
    if tx.returncode != 0:
        Engine.execute.log_fail(f"TxApp failed with return code {tx.returncode}")


def stop_rx_app(rx: Engine.execute.AsyncProcess):
    rx.process.send_signal(signal.SIGINT)
    rx.process.wait()


def remove_sent_file(file_path: str, app_path: str):
    # filepath "/x/y/z.yuv", app_path "/a/b/"
    # removes /a/b/z.yuv
    removal_path = f'{app_path}/{file_path.split("/")[-1]}'
    logging.debug(f"Removing: {removal_path}")
    os.remove(removal_path)


def run_rx_tx_with_file(file_path: str, build: str):
    app_path = os.path.join(build, "tests", "tools", "TestApp", "build")

    try:
        rx = run_rx_app(cwd=app_path)
        tx = run_tx_app(file_path=file_path, rx_pid=rx.process.pid, cwd=app_path)
        handle_tx_failure(tx)
    finally:
        stop_rx_app(rx)
        # TODO: Add checks for transmission errors
        remove_sent_file(file_path, app_path)
