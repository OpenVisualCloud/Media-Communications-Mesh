#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

if [[ $# -ne 0 ]]; then
    FFMPEG_VER="${1:-${FFMPEG_VER}}"
    shift
fi
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPOSITORY_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
BUILD_DIR="${BUILD_DIR:-${REPOSITORY_DIR}/_build}"

# shellcheck source="../scripts/common.sh"
. "${REPOSITORY_DIR}/scripts/common.sh"
lib_setup_ffmpeg_dir_and_version "${FFMPEG_VER:-7.0}"
export FFMPEG_DIR="${BUILD_DIR}/${FFMPEG_SUB_DIR}"

pushd "${FFMPEG_DIR}"
pkg-config --define-prefix --exists --print-errors libmcm_dp

# copy source files to allow the configure tool to find them
cp -f "${SCRIPT_DIR}/mcm_"* "${FFMPEG_DIR}/libavdevice/"
"${FFMPEG_DIR}/configure" --enable-shared --enable-mcm "$@"
popd

log_info "FFmpeg MCM plugin configuration completed."
log_info "\t${BUILD_DIR}/FFmpeg"
