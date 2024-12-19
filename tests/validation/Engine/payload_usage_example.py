# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

from payload import Audio, Video

payload = Video(width=3840, height=2160)

print(
    f"""payload info:
width={payload.width}
height={payload.height}
fps={payload.fps}
pixelFormat={payload.pixelFormat}
type={payload.payload_type}
"""
)

print(
    f"""Dict:
{payload.to_dict()}
"""
)


pl = Audio(channels=4)

print(
    f"""pl info:
channels={pl.channels}
type={pl.payload_type}
"""
)

print(
    f"""Dict:
{pl.to_dict()}
"""
)
