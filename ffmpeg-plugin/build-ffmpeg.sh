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

make -C "${FFMPEG_DIR}" -j "$(nproc)"
as_root make -C "${FFMPEG_DIR}" install

log_info "FFmpeg MCM plugin build completed."
log_info "\t${FFMPEG_DIR}"
