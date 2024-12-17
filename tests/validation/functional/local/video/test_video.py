# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh
import pytest
import os

import Engine.client_json
import Engine.connection
import Engine.connection_json
import Engine.execute
import Engine.payload
import Engine.engine_utils as utils
from Engine.media_files import yuv_files

def test_video(media):
    client = Engine.client_json.ClientJson()
    conn_mpg = Engine.connection.MultipointGroup()
    payload = Engine.payload.Video(width=3840, height=2160)
    connection = Engine.connection_json.ConnectionJson(connection=conn_mpg, payload=payload)

    utils.create_client_json(client, output_path="../tools/TestApp/build/client.json")
    utils.create_connection_json(connection, output_path="../tools/TestApp/build/connection.json")

    # Use a specified file from media_files.py
    media_file = yuv_files['i2160p25']['filename']
    media_file_path = os.path.join(media, media_file)

    utils.run_rx_tx_with_file(file_path=media_file_path, cwd="../tools/TestApp/build")