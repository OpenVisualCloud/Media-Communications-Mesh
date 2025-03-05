# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import logging
import signal
import subprocess
import time

from pathlib import Path

from .connection_json import ConnectionJson
from .client_json import ClientJson
from .execute import AsyncProcess, call, run, log_fail
from .integrity import calculate_yuv_frame_size, check_st20p_integrity

video_format_matches = {
    # file_format : payload format
    "YUV422PLANAR10LE": "yuv422p10le",
    "YUV422RFC4175PG2BE10": "yuv422p10rfc4175",
}


def video_file_format_to_payload_format(pixel_format: str) -> str:
    return video_format_matches.get(pixel_format, pixel_format) # matched if matches, else original


def create_client_json(build: str, client: ClientJson, filename: str = "client.json") -> None:
    logging.debug("Client JSON:")
    for line in client.to_json().splitlines():
        logging.debug(line)
    output_path = Path(build, "tests", "tools", "TestApp", "build", filename)
    logging.debug(f"Client JSON path: {output_path}")
    client.prepare_and_save_json(output_path=output_path)


def create_connection_json(build: str, connection: ConnectionJson, filename: str = "connection.json") -> None:
    logging.debug("Connection JSON:")
    for line in connection.to_json().splitlines():
        logging.debug(line)
    output_path = Path(build, "tests", "tools", "TestApp", "build", filename)
    logging.debug(f"Connection JSON path: {output_path}")
    connection.prepare_and_save_json(output_path=output_path)


def run_rx_app(
        client_cfg_file: str, connection_cfg_file: str,
        path_to_output_file: str, cwd: str, timeout: int = 0,
        mcm_media_proxy_port: int = -1) -> AsyncProcess:
    env = ({"MCM_MEDIA_PROXY_PORT": str(mcm_media_proxy_port)} if mcm_media_proxy_port != -1 else {})
    return call(
        f"./RxApp {client_cfg_file} {connection_cfg_file} {path_to_output_file}",
        cwd=cwd,
        timeout=timeout,
        env=env)


def run_tx_app(
    client_cfg_file: str,
    connection_cfg_file: str,
    path_to_input_file: str,
    cwd: str,
    testcmd: bool = True,
    mcm_media_proxy_port: int = -1
    ) -> subprocess.CompletedProcess:
    env = ({"MCM_MEDIA_PROXY_PORT": str(mcm_media_proxy_port)} if mcm_media_proxy_port != -1 else {})
    return run(
        f"./TxApp {client_cfg_file} {connection_cfg_file} {path_to_input_file}",
        cwd=cwd,
        env=env)


def handle_tx_failure(tx: subprocess.CompletedProcess) -> None:
    if tx.returncode != 0:
        log_fail(f"TxApp failed with return code {tx.returncode}")


def stop_rx_app(rx: AsyncProcess) -> None:
    rx.process.terminate()
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


def remove_sent_file(full_path: Path) -> None:
    try:
        full_path.unlink()
        logging.debug(f"Removed: {full_path}")
    # except makes the test pass if there's no file to remove
    except (FileNotFoundError, NotADirectoryError):
        logging.debug(f"Cannot remove. File does not exist: {full_path}")


def run_rx_tx_with_file(
        file_path: str, build: str, timeout: int = 0, media_info = {},
        rx_mp_port: int = -1, tx_mp_port: int = -1,
        rx_client_filename: str = "client.json",
        tx_client_filename: str = "client.json",
        rx_connection_filename: str = "connection.json",
        tx_connection_filename: str = "connection.json") -> None:
    app_path = Path(build, "tests", "tools", "TestApp", "build")

    try:
        rx_client_cfg_file = Path(app_path.resolve(), rx_client_filename)
        tx_client_cfg_file = Path(app_path.resolve(), tx_client_filename)
        rx_connection_cfg_file = Path(app_path.resolve(), rx_connection_filename)
        tx_connection_cfg_file = Path(app_path.resolve(), tx_connection_filename)

        output_file_path = Path(app_path, Path(file_path).name + "_MCMoutput.yuv").resolve()
        rx = run_rx_app(
            client_cfg_file=rx_client_cfg_file,
            connection_cfg_file=rx_connection_cfg_file,
            path_to_output_file=str(output_file_path),
            cwd=app_path,
            timeout=timeout,
            mcm_media_proxy_port=rx_mp_port)
        time.sleep(2) # 2 seconds for RxApp to spin up
        tx = run_tx_app(
            client_cfg_file=tx_client_cfg_file,
            connection_cfg_file=tx_connection_cfg_file,
            path_to_input_file=file_path,
            cwd=app_path,
            mcm_media_proxy_port=tx_mp_port)
        time.sleep(5) # 5 seconds for TxApp to shut down before handling it as a failure
        handle_tx_failure(tx)
        stop_rx_app(rx)
    finally:
        frame_size = calculate_yuv_frame_size(media_info.get("width"), media_info.get("height"), media_info.get("pixelFormat"))
        integrity_check = check_st20p_integrity(file_path, str(output_file_path), frame_size)
        logging.debug(f"Integrity: {integrity_check}")
        remove_sent_file(output_file_path)

        if not integrity_check:
            log_fail("At least one of the received frames has not passed the integrity test")
