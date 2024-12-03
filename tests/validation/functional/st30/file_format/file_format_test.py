# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

import time
import pytest, os
import Engine.SenderApp as sender
import Engine.ReceiverApp as receiver

from Engine.execute import call, wait
from Engine.media_files import audio_files


@pytest.mark.parametrize("file", audio_files.keys())
def test_st30_file_format(file):
    # sender
    sender_cfg = sender.get_sender_default()
    sender_cfg['file_name'] = audio_files[file]["filename"]
    sender_cfg['audio_format'] = file
    sender_cfg['payload_type'] = "st30"
    sender_cmd = sender.get_sender_cmd(sender_cfg)

    # receiver
    receiver_cfg = receiver.get_receiver_default()
    receiver_cfg['file_name'] = "test.pcm"
    receiver_cfg['audio_format'] = file
    receiver_cfg['payload_type'] = "st30"
    receiver_cmd = receiver.get_receiver_cmd(receiver_cfg)

    media_proxy_sender = call("sudo media_proxy -d 0000:4b:11.1 -t 9001", "/home/gta/mtl/Media-Communications-Mesh/tests/tools", 25, True)
    media_proxy_receiver = call("sudo media_proxy -d 0000:4b:11.2 -t 9000", "/home/gta/mtl/Media-Communications-Mesh/tests/tools", 25, True)

    time.sleep(5)
    receiver_proc = call(receiver_cmd, "/home/gta/mtl/Media-Communications-Mesh/tests/tools", 15, True)
    sender_proc = call(sender_cmd, "/home/gta/mtl/Media-Communications-Mesh/tests/tools", 15, True)

    wait(media_proxy_sender)
    wait(media_proxy_receiver)
    time.sleep(5)
    wait(receiver_proc)
    wait(sender_proc)

