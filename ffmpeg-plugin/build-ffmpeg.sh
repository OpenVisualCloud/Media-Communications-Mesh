#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
. "${SCRIPT_DIR}/../common.sh"

cp -f "${SCRIPT_DIR}/mcm_"* "${SCRIPT_DIR}/FFmpeg/libavdevice/"

make -C "${SCRIPT_DIR}/FFmpeg" -j "$(nproc)"
run_as_root_user make -C "${SCRIPT_DIR}/FFmpeg" install

prompt "FFmpeg MCM plugin build completed."
prompt "\t${SCRIPT_DIR}/FFmpeg"
