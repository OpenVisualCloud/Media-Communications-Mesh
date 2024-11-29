# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

import tests.Engine.SenderApp as sender
import tests.Engine.ReceiverApp as receiver

from tests.Engine.utils import file_format_to_pix_fmt
from tests.Engine.media_files import jxs_files, h264_files


@pytest.mark.parametrize("file", jxs_files.keys())
def test_st22_jxs(file):
    # sender
    sender_cfg = sender.get_sender_default()
    sender_cfg['file_name'] = jxs_files[file]["filename"]
    sender_cfg['pix_fmt'] = file_format_to_pix_fmt(jxs_files[file]["format"])
    sender_cfg['width'] = jxs_files[file]["width"]
    sender_cfg['height'] = jxs_files[file]["height"]
    sender_cfg['fps'] = jxs_files[file]["fps"]
    sender_cfg['payload_type'] = "st22"
    sender_cmd = sender.get_sender_cmd(sender_cfg)

    # receiver
    receiver_cfg = receiver.get_receiver_default()
    sender_cfg['payload_type'] = "st22"
    receiver_cmd = receiver.get_receiver_cmd(receiver_cfg)

    # TODO: run sender here
    # TODO: run receiver here
    # TODO: run media_proxies if needed


@pytest.mark.parametrize("file", h264_files.keys())
def test_st22_h264(file):
    # TODO: add the test contents here
    pass
