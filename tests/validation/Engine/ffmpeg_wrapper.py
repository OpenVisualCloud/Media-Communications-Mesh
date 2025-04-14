# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh

# TODO: consider how we can use https://github.com/kkroening/ffmpeg-python
# TODO: maybe we can write ffmpeg wrapper like scapy? e.g. every protocol is different class and it looks like this:
# Ether()/IP()/TCP() so maybe: ffmpeg_cmd = Input()/Transport()/Output() etc.

class FFmpeg:
    """
    FFMPEG wrapper with MCM plugin
    """
    def __init__(self, connection):
        self.conn = connection
        self._processes = []

    def start_ffmpeg(self, cmd):
        ffmpeg = self.conn.start_process(cmd)
        self._processes.append(ffmpeg)

    def prepare_st20_tx_cmd(self):
        pass