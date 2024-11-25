#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"

. "${SCRIPT_DIR}/scripts/common.sh"

# Set build type. ("Debug", "Release", or "RelWithDebInfo")
BUILD_TYPE="${BUILD_TYPE:-Debug}"
BUILD_UNIT_TESTS="${BUILD_UNIT_TESTS:-ON}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
RUN_UNIT_TESTS="${RUN_UNIT_TESTS:-ON}"

# Define BUILD_DIR if not set
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/_build}"

# Create the build directory if it doesn't exist
mkdir -p "${BUILD_DIR}"

# Use ccache
export CC="ccache gcc"
export CXX="ccache g++"

# Use Ninja build system for faster builds
cmake -G Ninja -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DBUILD_UNIT_TESTS="${BUILD_UNIT_TESTS}" \
    "${SCRIPT_DIR}"

# Build using all available cores
cmake --build "${BUILD_DIR}" -- -j$(nproc)

# Install (optional)
# run_as_root_user cmake --install "${BUILD_DIR}"
# run_as_root_user ln -s /usr/lib64/libbpf.so.1 /usr/lib/x86_64-linux-gnu/libbpf.so.1 2>/dev/null || true
# run_as_root_user ldconfig

# Run unit tests (optional)
if [[ "${RUN_UNIT_TESTS}" == "ON" ]]; then
    export LD_LIBRARY_PATH="${PREFIX_DIR}/usr/local/lib:/usr/local/lib"
    "${BUILD_DIR}/bin/sdk_unit_tests"
    "${BUILD_DIR}/bin/media_proxy_unit_tests"
    "${BUILD_DIR}/bin/conn_rdma_unit_tests"
    "${BUILD_DIR}/bin/conn_rdma_real_ep_test"
fi

export CC=""
export CXX=""

prompt "Build Succeeded"

