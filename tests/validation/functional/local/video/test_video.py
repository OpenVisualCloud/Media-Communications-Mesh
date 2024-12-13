# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh
import logging

import Engine.client_json
import Engine.connection
import Engine.connection_json
import Engine.execute
import Engine.payload


def test_video():
    client = Engine.client_json.ClientJson()
    conn_mpg = Engine.connection.MultipointGroup()
    payload = Engine.payload.Video(width=3840, height=2160)
    connection = Engine.connection_json.ConnectionJson(connection=conn_mpg, payload=payload)

    logging.debug("Client JSON:")
    for line in client.to_json().splitlines():
        logging.debug(line)
    client.prepare_and_save_json(output_path="../tools/TestApp/build/client.json")
    logging.debug("Connection JSON:")
    for line in connection.to_json().splitlines():
        logging.debug(line)
    connection.prepare_and_save_json(output_path="../tools/TestApp/build/connection.json")

    # Create a 1MB random file (for testing purposes)
    Engine.execute.run("dd if=/dev/urandom of=/tmp/random_file.bin bs=1M count=1")
    try:
        rx = Engine.execute.call("./RxApp", cwd="../tools/TestApp/build", timeout=0)
        tx = Engine.execute.run(
            f"./TxApp /tmp/random_file.bin {rx.process.pid}", testcmd=True, cwd="../tools/TestApp/build"
        )
        if tx.returncode != 0:
            Engine.execute.log_fail(f"TxApp failed with return code {tx.returncode}")
    finally:
        rx.process.send_signal(2)  # SIGINT
        rx.process.wait()
