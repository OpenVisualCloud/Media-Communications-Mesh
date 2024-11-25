#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

FFMPEG_VER="${1:-${FFMPEG_VER}}"
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPOSITORY_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
BUILD_DIR="${BUILD_DIR:-${REPOSITORY_DIR}/_build}"

# shellcheck source="../scripts/common.sh"
. "${REPOSITORY_DIR}/scripts/common.sh"
lib_setup_ffmpeg_dir_and_version "${FFMPEG_VER:-7.0}"
export FFMPEG_DIR="${BUILD_DIR}/${FFMPEG_SUB_DIR}"

git_download_strip_unpack "FFmpeg/FFmpeg" "release/${FFMPEG_VER}" "${FFMPEG_DIR}"
patch --directory="${FFMPEG_DIR}" -p1 -i <(cat "${SCRIPT_DIR}/${FFMPEG_VER}/"*.patch)

log_info "FFmpeg ${FFMPEG_VER} cloned and patched successfully."
log_info "\t${FFMPEG_DIR}"
