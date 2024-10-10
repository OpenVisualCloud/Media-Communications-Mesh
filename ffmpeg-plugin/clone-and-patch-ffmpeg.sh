#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPOSITORY_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"

# shellcheck source="../scripts/common.sh"
. "${REPOSITORY_DIR}/scripts/common.sh"

# Default to latest 7.0
FFMPEG_VER="${FFMPEG_VER:-7.0}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Clone FFmpeg:
git clone --branch "release/${FFMPEG_VER}" --depth 1 https://github.com/FFmpeg/FFmpeg.git "${BUILD_DIR}/FFmpeg"

patch --directory="${BUILD_DIR}/FFmpeg" -p1 -i <(cat "${SCRIPT_DIR}/${FFMPEG_VER}/"*.patch)

prompt "FFmpeg ${FFMPEG_VER} cloned and patched successfully."
prompt "\t${BUILD_DIR}/FFmpeg"
