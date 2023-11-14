# SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

# generate test source
ffmpeg -y -f lavfi -i testsrc=r=30:decimals=3:s=1920x1080 -pix_fmt yuv422p10be -f rawvideo input-a.fifo
ffmpeg -y -f lavfi -i testsrc2=r=30:decimals=3:s=1920x1080 -pix_fmt yuv422p10be -f rawvideo input-b.fifo

# plaback result
ffplay -f rawvideo -pixel_format yuv422p10be -video_size 1920x1080 -i output-a.fifo
ffplay -f rawvideo -pixel_format yuv422p10be -video_size 1920x1080 -i output-b.fifo
