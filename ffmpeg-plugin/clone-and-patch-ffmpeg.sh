#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
. "${SCRIPT_DIR}/../common.sh"

# First parameter or default to latest 6.1
ffmpeg_ver="${1:-6.1}"

rm -rf "${SCRIPT_DIR}/FFmpeg"
git clone https://github.com/FFmpeg/FFmpeg.git "${SCRIPT_DIR}/FFmpeg"
git -C "${SCRIPT_DIR}/FFmpeg" checkout release/"${ffmpeg_ver}"
git -C "${SCRIPT_DIR}/FFmpeg" apply "${SCRIPT_DIR}/${ffmpeg_ver}/"*.patch

prompt "FFmpeg ${ffmpeg_ver} cloned and patched successfully."
prompt "\t${SCRIPT_DIR}/FFmpeg"
