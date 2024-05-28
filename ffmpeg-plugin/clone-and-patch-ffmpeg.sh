#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -e

if [ -n "$1" ]; then
	ffmpeg_ver=$1
else
	# default to latest 6.1
	ffmpeg_ver=6.1
fi

rm FFmpeg -rf
git clone https://github.com/FFmpeg/FFmpeg.git
cd FFmpeg
git checkout release/"$ffmpeg_ver"
git apply ../"$ffmpeg_ver"/*.patch
cd ..

echo "FFmpeg $ffmpeg_ver cloned and patched successfully"
