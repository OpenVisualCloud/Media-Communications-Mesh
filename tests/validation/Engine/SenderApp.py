# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

import os
from connection_json import ConnectionJson

# TODO: Align with connection_json.py


def get_sender_default(app_path = f"{os.environ['build']}/tests/tools/sender_val") -> dict:
    return {
        "media_proxy_port": 9000,
        "app_path": app_path,
        "file_name": "input.yuv",
        "width": 1920,
        "height": 1080,
        "fps": 25,
        "pix_fmt": "yuv422p10le",
        "recv_ip": "192.168.2.1",
        "recv_port": 8001,
        "send_ip": "192.168.2.2",
        "send_port": 8002,
        "protocol_type": "auto",
        "payload_type": "st20",
        "socketpath": "/run/mcm/mcm_rx_memif.sock",
        "interfaceid": 0,
        "loop": 0,
        "audio_channels": 1,
        "audio_sample": 48000,
        "audio_format": "PCM16",
        "audio_packet": "1"
    }


def get_sender_cmd(config: dict) -> str:
    return """MCM_MEDIA_PROXY_PORT={media_proxy_port} \
    {app_path} \
    -w {width} \
    -h {height} \
    -f {fps} \
    -x {pix_fmt} \
    -s {send_ip} \
    -p {send_port} \
    -r {recv_ip} \
    -i {recv_port} \
    -b {file_name}
    """.format(**config)

def get_sender_st20_cmd(config: dict) -> str:
    return """sudo MCM_MEDIA_PROXY_PORT={media_proxy_port} \
    {app_path} \
    -w {width} \
    -h {height} \
    -f {fps} \
    -x {pix_fmt} \
    -s {send_ip} \
    -r {recv_ip} \
    -i {recv_port} \
    -b {file_name}
    """.format(**config)

def get_sender_st30_cmd(config: dict) -> str:
    return """sudo MCM_MEDIA_PROXY_PORT={media_proxy_port} \
    {app_path} \
    -ac {audio_channels} \
    -as {audio_sample} \
    -af {audio_format} \
    -ap {audio_packet} \
    -s {send_ip} \
    -r {recv_ip} \
    -i {recv_port} \
    -b {file_name}
    """.format(**config)
