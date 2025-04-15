# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh

# generates typed-ffmpeg's extra_options for Media Transport Library based on AVOption elements from
# https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/ecosystem/ffmpeg_plugin/mtl_st20p_rx.c
# each Rx has MTL_RX_DEV_ARGS and MTL_RX_PORT_ARGS + the options provided in AVOption elements

from mtl_common import MtlCommonRx

class MtlSt20pRx(MtlCommonRx):
    def __init__(
        self,
        video_size: str = "1920x1080",
        # pix_fmt: str = "", # duplicates with pixel_format, which is clearer
        pixel_format: str = "yuv422p10le",
        fps: float = 59.94,
        timeout_s: int = 0,
        init_retry: int = 5,
        fb_cnt: int = 3,
        gpu_direct: bool = False, # FIXME: this adds -nogpu_direct, which is unwanted
        gpu_driver: int = 0,
        gpu_device: int = 0,
    ):
        self.video_size = video_size
        self.pixel_format = pixel_format
        self.fps = fps
        self.timeout_s = timeout_s
        self.init_retry = init_retry
        self.fb_cnt = fb_cnt
        self.gpu_direct = gpu_direct
        self.gpu_driver = gpu_driver
        self.gpu_device = gpu_device
        super().__init__()
