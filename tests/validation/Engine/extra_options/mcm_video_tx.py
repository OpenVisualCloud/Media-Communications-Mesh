# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh

# generates typed-ffmpeg's extra_options for Media Communications Mesh based on
# https://github.com/OpenVisualCloud/Media-Communications-Mesh/blob/main/ffmpeg-plugin/mcm_video_tx.c

class McmVideoTx:
    def __init__(
        self,
        buf_queue_cap: int = 8,
        conn_delay: int = 0,
        conn_type: str = "multipoint-group", # 'multipoint-group' or 'st2110'
        urn: str = "192.168.97.1",
        ip_addr: str = "192.168.96.2",
        port: int = 9001,
        transport: str = "st2110-20",
        payload_type: int = 112,
        transport_pixel_format: str = "yuv422p10rfc4175",
        socket_name: str = None,
        interface_id: int = 0,
        video_size: str = "1920x1080",
        pixel_format: str = "yuv422p10le",
        frame_rate: str = "25",
    ):
        self.buf_queue_cap = buf_queue_cap
        self.conn_delay = conn_delay
        self.conn_type = conn_type
        self.urn = urn
        self.ip_addr = ip_addr
        self.port = port
        self.transport = transport
        self.payload_type = payload_type
        self.transport_pixel_format = transport_pixel_format
        self.socket_name = socket_name
        self.interface_id = interface_id
        self.video_size = video_size
        self.pixel_format = pixel_format
        self.frame_rate = frame_rate

    def get_items(self):
        response = {}
        for key, value in self.__dict__.items():
            if value is not None:
                response[key] = value
        return response
