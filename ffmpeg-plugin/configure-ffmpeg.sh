#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPOSITORY_DIR="$(readlink -f "$(dirname -- "${SCRIPT_DIR}/..")")"
. "${REPOSITORY_DIR}/common.sh"

cd "${REPOSITORY_DIR}/out/FFmpeg"
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig" pkg-config --exists --print-errors libmcm_dp

# copy source files to allow the configure tool to find them
#cp -f ../mcm_* ./libavdevice/

"${REPOSITORY_DIR}/out/FFmpeg/configure" --enable-shared --enable-mcm $@
cd "${OLDPWD}"

prompt "FFmpeg MCM plugin configuration completed."
prompt "\t${REPOSITORY_DIR}/out/FFmpeg"
