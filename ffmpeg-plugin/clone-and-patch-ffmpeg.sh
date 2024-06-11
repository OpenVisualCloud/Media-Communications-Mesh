#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPOSITORY_DIR="$(readlink -f "$(dirname -- "${SCRIPT_DIR}/..")")"
. "${REPOSITORY_DIR}/common.sh"

# First parameter or default to latest 6.1
ffmpeg_ver="${1:-6.1}"

rm -rf "${REPOSITORY_DIR}/out/FFmpeg"
mkdir -p "${REPOSITORY_DIR}/out/FFmpeg"
git clone https://github.com/FFmpeg/FFmpeg.git "${REPOSITORY_DIR}/out/FFmpeg"
git -C "${REPOSITORY_DIR}/out/FFmpeg" checkout release/"${ffmpeg_ver}"
git -C "${REPOSITORY_DIR}/out/FFmpeg" apply "${SCRIPT_DIR}/${ffmpeg_ver}/"*.patch

prompt "FFmpeg ${ffmpeg_ver} cloned and patched successfully."
prompt "\t${REPOSITORY_DIR}/out/FFmpeg"
