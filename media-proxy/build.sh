#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
. "${SCRIPT_DIR}/../common.sh"

# Set build type. ("Debug" or "Release")
BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

cmake -B "${SCRIPT_DIR}/out" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    "${SCRIPT_DIR}"
cmake --build "${SCRIPT_DIR}/out" -j

# Install
run_as_root_user cmake --install "${SCRIPT_DIR}/out"
ldconfig
