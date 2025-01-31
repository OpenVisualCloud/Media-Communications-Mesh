# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import os

import Engine.client_json
import Engine.connection
import Engine.connection_json
import Engine.engine_mcm as utils
import Engine.execute
import Engine.payload
import pytest
from Engine.media_files import audio_files


@pytest.mark.parametrize("file", audio_files.values())
def test_audio(build: str, media: str, file: dict):
    client = Engine.client_json.ClientJson()
    conn_mpg = Engine.connection.MultipointGroup()
    payload = Engine.payload.Audio(
        channels=file["channels"],
        sampleRate=file["sample_rate"],
        audio_format=file["format"],
    )
    connection = Engine.connection_json.ConnectionJson(
        connection=conn_mpg, payload=payload
    )

    utils.create_client_json(build, client)
    utils.create_connection_json(build, connection)

    media_file = file["filename"]
    media_file_path = os.path.join(media, media_file)

    utils.run_rx_tx_with_file(file_path=media_file_path, build=build)
