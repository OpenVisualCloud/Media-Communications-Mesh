# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import os
import pytest

from Engine.client_json import ClientJson
from Engine.connection import MultipointGroup
from Engine.connection_json import ConnectionJson
from Engine.payload import Video
from Engine.engine_mcm import video_file_format_to_payload_format, create_client_json, create_connection_json, run_rx_tx_with_file
from Engine.media_files import yuv_files


@pytest.mark.parametrize("video_type", [k for k in yuv_files.keys()])
@pytest.mark.parametrize("test_type_setter", ["rdma"], indirect=True, ids=["cluster"])
def test_video(test_type_setter, test_type_checker, build_TestApp, build: str, media_proxy_cluster, media: str, video_type: str, tx_mp_port:int = 8002, rx_mp_port: int = 8003) -> None:
    media_proxy_cluster

    rx_client = ClientJson(apiConnectionString=f"Server=127.0.0.1; Port={tx_mp_port}")
    rx_client_filename = "rx_client.json"
    create_client_json(build, rx_client, rx_client_filename)

    tx_client = ClientJson(apiConnectionString=f"Server=127.0.0.1; Port={rx_mp_port}")
    tx_client_filename = "tx_client.json"
    create_client_json(build, tx_client, tx_client_filename)

    conn_mpg = MultipointGroup(urn="abc") # FIXME: Temporary workaround for SDBQ-1949
    payload = Video(
        width=yuv_files[video_type]["width"],
        height=yuv_files[video_type]["height"],
        fps=yuv_files[video_type]["fps"],
        pixelFormat=video_file_format_to_payload_format(yuv_files[video_type]["file_format"]),
    )
    connection = ConnectionJson(
        connection=conn_mpg, payload=payload
    )

    create_connection_json(build, connection)

    # Use a specified file from media_files.py
    media_file = yuv_files[video_type]["filename"]
    media_file_path = os.path.join(media, media_file)

    media_info = {
        "width": payload.width,
        "height": payload.height,
        "fps": payload.fps,
        "pixelFormat": payload.pixelFormat,
    }

    run_rx_tx_with_file(
        file_path=media_file_path,
        build=build,
        timeout=0,
        media_info=media_info,
        rx_mp_port=rx_mp_port,
        tx_mp_port=tx_mp_port,
        rx_client_filename = rx_client_filename,
        tx_client_filename = tx_client_filename,
        )
