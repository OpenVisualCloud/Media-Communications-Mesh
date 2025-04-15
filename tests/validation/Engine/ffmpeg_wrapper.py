# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh

# MTL extra_options usage:
# >>> import ffmpeg # (typed-ffmpeg library)
# fi = ffmpeg.input(filename="-", extra_options={"ptime": "1ms"}) # extra_options to be used as a mean of adding the parameters
# fo = (fi.output(filename="xyz"))
# fo.compile()
# ['ffmpeg', '-ptime', '1ms', '-i', '-', 'xyz']

# >>> import ffmpeg
# >>> import mtl_st20p_rx
# >>> st20p_rx = mtl_st20p_rx.MtlSt20pRx(video_size="3840x2160", timeout_s=4)
# >>> st20p_rx.get_items()
# {'video_size': '3840x2160', 'fps': 59.94, 'timeout_s': 4, 'init_retry': 5, 'fb_cnt': 3, 'gpu_direct': False, 'gpu_driver': 0, 'gpu_device': 0, 'rx_queues': -1, 'tx_queues': -1, 'udp_port': 20000, 'payload_type': 112}

# >>> fi = ffmpeg.input(filename="xyz", extra_options=st20p_rx.get_items())
# >>> fo = fi.output(filename="abc")
# >>> fo.compile()
# ['ffmpeg', '-video_size', '3840x2160', '-fps', '59.94', '-timeout_s', '4', '-init_retry', '5', '-fb_cnt', '3', '-nogpu_direct', '-gpu_driver', '0', '-gpu_device', '0', '-rx_queues', '-1', '-tx_queues', '-1', '-udp_port', '20000', '-payload_type', '112', '-i', 'xyz', 'abc']
# >>> ' '.join(fo.compile())
# 'ffmpeg -video_size 3840x2160 -fps 59.94 -timeout_s 4 -init_retry 5 -fb_cnt 3 -nogpu_direct -gpu_driver 0 -gpu_device 0 -rx_queues -1 -tx_queues -1 -udp_port 20000 -payload_type 112 -i xyz abc'

# TODO: Continue adding above to below functions

import ffmpeg as ff # typed-ffmpeg

class FFmpeg:
    """
    FFmpeg wrapper with MCM plugin
    """
    def __init__(self, connection):
        self.conn = connection
        self._processes = []

    def start_ffmpeg(self, cmd):
        ffmpeg = self.conn.start_process(cmd)
        self._processes.append(ffmpeg)

    def prepare_st20_tx_cmd(self):
        # TODO: Remove this example
        expected_cmd = 'ffmpeg -video_size 1920x1080 -i example.yuv -buf_queue_cap 8 -conn_delay 0 -conn_type multipoint-group -urn 192.168.97.1 -ip_addr 192.168.96.2 -port 9001 -transport st2110-20 -payload_type 96 -transport_pixel_format yuv422p10rfc4175 -interface_id 0 -video_size 1920x1080 -pixel_format yuv422p10le -frame_rate 25'

        from extra_options.mcm_video_tx import McmVideoTx
        mcm_vtx = McmVideoTx(video_size="1920x1080", payload_type=96)
        fi = ff.input(filename="example.yuv", extra_options={"video_size": "1920x1080"}) # TODO: Try to change extra_options from here to .video_size()
        fo = fi.output(filename="", extra_options=mcm_vtx.get_items())
        cmd = " ".join(fo.compile())[:-1]
        print(f"Produced command >{cmd}< is", end=" ")
        if cmd == expected_cmd:
            print("as expected")
        else:
            print("NOT AS EXPECTED")
        pass
