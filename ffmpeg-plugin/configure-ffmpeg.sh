#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
. "${SCRIPT_DIR}/../common.sh"

JPEGXS_FLAGS=( )
JPEGXS_ENABLED="${JPEGXS_ENABLED:-1}"

if [ "$JPEGXS_ENABLED" == "1" ]
then
    pushd "${BUILD_DIR}/jpegxs/Build/linux"
    run_as_root_user ./build.sh release install
    JPEGXS_FLAGS+=( "--enable-libsvtjpegxs" )
    popd
fi

PKG_CONFIG_PATH="/usr/local/lib/pkgconfig" pkg-config --exists --print-errors libmcm_dp

# copy source files to allow the configure tool to find them
#cp -f ../mcm_* ./libavdevice/

pushd "${BUILD_DIR}/FFmpeg"
"${BUILD_DIR}/FFmpeg/configure" \
    --disable-shared \
    --enable-static \
    --enable-mcm ${JPEGXS_FLAGS[@]} $@
popd

prompt "FFmpeg MCM plugin configuration completed."
prompt "\t${BUILD_DIR}/FFmpeg"
