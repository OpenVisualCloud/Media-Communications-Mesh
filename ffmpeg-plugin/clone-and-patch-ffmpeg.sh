#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
. "${SCRIPT_DIR}/../common.sh"

# Default to latest 6.1
FFMPEG_VER="${FFMPEG_VER:-6.1}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Clone FFmpeg:
git clone --branch "release/${FFMPEG_VER}" --depth 1 https://github.com/FFmpeg/FFmpeg.git "${BUILD_DIR}/FFmpeg"

git -C "${BUILD_DIR}/FFmpeg" apply --whitespace=fix "${SCRIPT_DIR}/${FFMPEG_VER}/"*.patch

prompt "FFmpeg ${FFMPEG_VER} cloned and patched successfully."
prompt "\t${BUILD_DIR}/FFmpeg"
