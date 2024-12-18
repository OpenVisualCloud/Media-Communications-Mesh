# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh
import os
import pytest

import Engine.client_json
import Engine.connection
import Engine.connection_json
import Engine.engine_mcm as utils
import Engine.execute
import Engine.payload
from Engine.media_files import yuv_files
from Engine.fixtures_mcm import media_proxy_single, build_TestApp


@pytest.mark.fixture(media_proxy_single)
@pytest.mark.fixture(build_TestApp)
def test_video(build: str, media: str):
    client = Engine.client_json.ClientJson()
    conn_mpg = Engine.connection.MultipointGroup()
    payload = Engine.payload.Video(width=3840, height=2160)
    connection = Engine.connection_json.ConnectionJson(connection=conn_mpg, payload=payload)

    utils.create_client_json(build, client)
    utils.create_connection_json(build, connection)

    # Use a specified file from media_files.py
    media_file = yuv_files["i2160p25"]["filename"]
    media_file_path = os.path.join(media, media_file)

    utils.run_rx_tx_with_file(file_path=media_file_path, build=build)
