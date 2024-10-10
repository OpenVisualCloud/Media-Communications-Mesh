#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPOSITORY_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"

# shellcheck source="../scripts/common.sh"
. "${REPOSITORY_DIR}/scripts/common.sh"

pushd "${BUILD_DIR}/FFmpeg"
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig" pkg-config --exists --print-errors libmcm_dp

# copy source files to allow the configure tool to find them
#cp -f ../mcm_* ./libavdevice/

"${BUILD_DIR}/FFmpeg/configure" --enable-shared --enable-mcm "$@"
popd

prompt "FFmpeg MCM plugin configuration completed."
prompt "\t${BUILD_DIR}/FFmpeg"
