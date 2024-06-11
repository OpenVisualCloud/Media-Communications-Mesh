#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPOSITORY_DIR="$(readlink -f "$(dirname -- "${SCRIPT_DIR}/..")")"
. "${REPOSITORY_DIR}/common.sh"

cp -f "${SCRIPT_DIR}/mcm_"* "${REPOSITORY_DIR}/out/FFmpeg/libavdevice/"

make -C "${REPOSITORY_DIR}/out/FFmpeg/" -j "$(nproc)"
run_as_root_user make -C "${REPOSITORY_DIR}/out/FFmpeg/" install

prompt "FFmpeg MCM plugin build completed."
prompt "\t${REPOSITORY_DIR}/out/FFmpeg"
