# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh

# generates typed-ffmpeg's extra_options for Media Transport Library based on AVOption elements from
# https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/ecosystem/ffmpeg_plugin/mtl_st30p_rx.c
# each Rx has MTL_RX_DEV_ARGS and MTL_RX_PORT_ARGS + the options provided in AVOption elements

from mtl_common import MtlCommonRx

# TODO: Think about Enums for sample rate, channels, pcm_fmt and ptime

class MtlSt30pRx(MtlCommonRx):
    def __init__(
        self,
        fb_cnt: int = 3,
        timeout_s: int = 0,
        init_retry: int = 5,
        sample_rate: int = 48000,
        channels: int = 2,
        pcm_fmt: str = None,
        ptime: str = None,
    ):
        self.fb_cnt = fb_cnt
        self.timeout_s = timeout_s
        self.init_retry = init_retry
        self.sample_rate = sample_rate
        self.channels = channels
        self.pcm_fmt = pcm_fmt
        self.ptime = ptime
        super().__init__()
