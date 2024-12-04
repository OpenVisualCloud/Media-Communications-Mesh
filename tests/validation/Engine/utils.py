# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

file_format_map = {
    "YUV_444_10bit": "yuv444p10le",
    "YUV_444_8bit": "yuv444p",
    "YUV_422_10bit": "yuv422p10le",
    "YUV_422_8bit": "yuv422p",
    "YUV_420_10bit": "yuv420p10le",
    "YUV_420_8bit": "yuv420p",
}


def file_format_to_pix_fmt(file_format=""):
    global file_format_map
    return file_format_map.get(file_format, "yuv422p10le")
