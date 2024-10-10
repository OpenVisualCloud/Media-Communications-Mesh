#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
. "${SCRIPT_DIR}/scripts/common.sh"

# Set build type. ("Debug" or "Release")
BUILD_TYPE="${BUILD_TYPE:-Release}"
# To disable the building of unit tests, set the value to "OFF".
BUILD_UNIT_TESTS="${BUILD_UNIT_TESTS:-ON}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

cmake -B "${SCRIPT_DIR}/out" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DBUILD_UNIT_TESTS="${BUILD_UNIT_TESTS}" \
    "${SCRIPT_DIR}"
cmake --build "${SCRIPT_DIR}/out" -j

# Install
run_as_root_user cmake --install "${SCRIPT_DIR}/out"
run_as_root_user ln -s /usr/lib64/libbpf.so.1 /usr/lib/x86_64-linux-gnu/libbpf.so.1 2>/dev/null || true
run_as_root_user ldconfig

# Run unit tests
LD_LIBRARY_PATH="${PREFIX_DIR}/usr/local/lib:/usr/local/lib" "${SCRIPT_DIR}/out/bin/mcm_unit_tests"
