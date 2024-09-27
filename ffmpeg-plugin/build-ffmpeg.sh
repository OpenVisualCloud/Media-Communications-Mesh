#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
. "${SCRIPT_DIR}/../common.sh"

cp -f "${SCRIPT_DIR}/mcm_"* "${BUILD_DIR}/FFmpeg/libavdevice/"

make -C "${BUILD_DIR}/FFmpeg/" -j "$(nproc)"
run_as_root_user make -C "${BUILD_DIR}/FFmpeg/" install
ldconfig

prompt "FFmpeg MCM plugin build completed."
prompt "\t${BUILD_DIR}/FFmpeg"
