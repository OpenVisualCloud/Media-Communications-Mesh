#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
. "${SCRIPT_DIR}/../common.sh"

# Default to latest 6.1
FFMPEG_VER="${FFMPEG_VER:-6.1}"
# Default to latest 0.9
JPEGXS_VER="${JPEGXS_VER:-e1030c6c8ee2fb05b76c3fa14cccf8346db7a1fa}"
JPEGXS_ENABLED="${JPEGXS_ENABLED:-1}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Clone FFmpeg:
git clone --branch "release/${FFMPEG_VER}" --depth 1 https://github.com/FFmpeg/FFmpeg.git "${BUILD_DIR}/FFmpeg"

# Clone JPEG XS if JPEGXS_VER is set, else skip:
if [ "$JPEGXS_ENABLED" == "1" ]; then
    git clone https://github.com/OpenVisualCloud/SVT-JPEG-XS "${BUILD_DIR}/jpegxs"
    git -C "${BUILD_DIR}/jpegxs" checkout "${JPEGXS_VER}"
    git -C "${BUILD_DIR}/FFmpeg" apply --whitespace=fix ${BUILD_DIR}/jpegxs/ffmpeg-plugin/0001-Enable-JPEG-XS-codec-type.patch
    git -C "${BUILD_DIR}/FFmpeg" apply --whitespace=fix ${BUILD_DIR}/jpegxs/ffmpeg-plugin/0002-Allow-JPEG-XS-to-be-stored-in-mp4-mkv-container.patch
    git -C "${BUILD_DIR}/FFmpeg" apply --whitespace=fix ${BUILD_DIR}/jpegxs/ffmpeg-plugin/0003-svt-jpegxs-encoder-support.patch
    git -C "${BUILD_DIR}/FFmpeg" apply --whitespace=fix ${BUILD_DIR}/jpegxs/ffmpeg-plugin/0004-svt-jpegxs-decoder-support.patch
fi

git -C "${BUILD_DIR}/FFmpeg" apply --whitespace=fix "${SCRIPT_DIR}/${FFMPEG_VER}/"*.patch

prompt "FFmpeg ${FFMPEG_VER} cloned and patched successfully."
prompt "\t${BUILD_DIR}/FFmpeg"
