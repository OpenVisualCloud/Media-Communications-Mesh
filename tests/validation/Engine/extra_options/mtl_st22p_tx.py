# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh

# generates typed-ffmpeg's extra_options for Media Transport Library based on AVOption elements from
# https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/ecosystem/ffmpeg_plugin/mtl_st22p_tx.c
# each Tx has MTL_TX_DEV_ARGS and MTL_TX_PORT_ARGS + the options provided in AVOption elements

from mtl_common import MtlCommonTx

class MtlSt22pTx(MtlCommonTx):
    def __init__(
        self,
        fb_cnt: int = 3,
        bpp: float = 3.0,
        codec_thread_cnt: int = 0,
        st22_codec: str = None,
    ):
        self.fb_cnt = fb_cnt
        self.bpp = bpp
        self.codec_thread_cnt = codec_thread_cnt
        self.st22_codec = st22_codec
        super().__init__()
