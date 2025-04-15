# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh

# generates typed-ffmpeg's extra_options for Media Communications Mesh based on
# https://github.com/OpenVisualCloud/Media-Communications-Mesh/blob/main/ffmpeg-plugin/mcm_audio_rx.c

class McmAudioRx:
    def __init__(
        self,
        buf_queue_cap: int = 16,
        conn_delay: int = 0,
        conn_type: str = "multipoint-group", # 'multipoint-group' or 'st2110'
        urn: str = "192.168.97.1",
        ip_addr: str = "239.168.68.190",
        port: int = 9001,
        mcast_sip_addr: str = "",
        # transport: str = "st2110-30", # happens automatically
        payload_type: int = 111,
        socket_name: str = None,
        interface_id: int = 0,
        channels: int = 2,
        sample_rate: int = 48000,
        ptime: str = "1ms",
    ):
        self.buf_queue_cap = buf_queue_cap
        self.conn_delay = conn_delay
        self.conn_type = conn_type
        self.urn = urn
        self.ip_addr = ip_addr
        self.port = port
        self.mcast_sip_addr = mcast_sip_addr
        self.payload_type = payload_type
        self.socket_name = socket_name
        self.interface_id = interface_id
        self.channels = channels,
        self.sample_rate = sample_rate,
        self.ptime = ptime,

    def get_items(self):
        response = {}
        for key, value in self.__dict__.items():
            if value is not None:
                response[key] = value
        return response
