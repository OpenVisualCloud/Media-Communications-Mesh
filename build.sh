#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"

. "${SCRIPT_DIR}/scripts/common.sh"

# Set build type. ("Debug" or "Release")
# To disable the building of unit tests, set the value to "OFF".

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_UNIT_TESTS="${BUILD_UNIT_TESTS:-ON}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DBUILD_UNIT_TESTS="${BUILD_UNIT_TESTS}" \
    "${SCRIPT_DIR}"
cmake --build "${BUILD_DIR}" -j

# Install
run_as_root_user cmake --install "${BUILD_DIR}"
run_as_root_user ln -s /usr/lib64/libbpf.so.1 /usr/lib/x86_64-linux-gnu/libbpf.so.1 2>/dev/null || true
run_as_root_user ldconfig

# Run unit tests
export LD_LIBRARY_PATH="${PREFIX_DIR}/usr/local/lib:/usr/local/lib"
"${BUILD_DIR}/bin/sdk_unit_tests"
"${BUILD_DIR}/bin/media_proxy_unit_tests"

prompt "Build Succeeded"
