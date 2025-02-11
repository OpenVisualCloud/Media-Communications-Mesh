# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import logging
import signal
import subprocess

from pathlib import Path

import Engine.client_json
import Engine.connection
import Engine.connection_json
import Engine.execute
import Engine.payload


def create_client_json(build: str, client: Engine.client_json.ClientJson):
    logging.debug("Client JSON:")
    for line in client.to_json().splitlines():
        logging.debug(line)
    output_path = Path(build, "tests", "tools", "TestApp", "build", "client.json")
    logging.debug(f"Client JSON path: {output_path}")
    client.prepare_and_save_json(output_path=output_path)


def create_connection_json(build: str, connection: Engine.connection_json.ConnectionJson):
    logging.debug("Connection JSON:")
    for line in connection.to_json().splitlines():
        logging.debug(line)
    output_path = Path(build, "tests", "tools", "TestApp", "build", "connection.json")
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


def remove_sent_file(file_path: str, app_path: str) -> None:
    # filepath "/x/y/z.yuv", app_path "/a/b/"
    # removes /a/b/z.yuv
    removal_path = Path(app_path, Path(file_path).name)
    try:
        removal_path.unlink()
        logging.debug(f"Removed: {removal_path}")
    # except makes the test pass if there's no file to remove
    except (FileNotFoundError, NotADirectoryError):
        logging.debug(f"Cannot remove. File does not exist: {removal_path}")


def run_rx_tx_with_file(file_path: str, build: str):
    app_path = Path(build, "tests", "tools", "TestApp", "build")

    try:
        rx = run_rx_app(cwd=app_path)
        tx = run_tx_app(file_path=file_path, rx_pid=rx.process.pid, cwd=app_path)
        handle_tx_failure(tx)
    finally:
        stop_rx_app(rx)
        # TODO: Add checks for transmission errors
        remove_sent_file(file_path, app_path)
