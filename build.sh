#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

# Passing custom path as a parameter to this script will install
# SDK in to default path and the one pointed as $1

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"

# shellcheck source="scripts/common.sh"
. "${SCRIPT_DIR}/scripts/common.sh"

# Set build type. ("Debug" or "Release")
# To disable the building of unit tests, set the value to "OFF".

MCM_BUILD_DIR="${MCM_BUILD_DIR:-${BUILD_DIR}/mcm}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_UNIT_TESTS="${BUILD_UNIT_TESTS:-ON}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_UNIT_TESTS="${BUILD_UNIT_TESTS}" \
    -B "${MCM_BUILD_DIR}" -S "${SCRIPT_DIR}"
make -j "${NPROC}" -C "${MCM_BUILD_DIR}"

if [[ $# -ne 0 ]]; then
    DESTDIR="${1:-$DESTDIR}" make -C "${MCM_BUILD_DIR}" install
fi
as_root make -C "${MCM_BUILD_DIR}" install

as_root ln -s /usr/lib64/libbpf.so.1 /usr/lib/x86_64-linux-gnu/libbpf.so.1 2>/dev/null || true
as_root ldconfig

# Run unit tests
if [ "${BUILD_UNIT_TESTS}" = "ON" ]; then
    export LD_LIBRARY_PATH="${PREFIX_DIR}/usr/local/lib:/usr/local/lib64"
    "${MCM_BUILD_DIR}/bin/sdk_unit_tests"
    "${MCM_BUILD_DIR}/bin/media_proxy_unit_tests"
    # "${MCM_BUILD_DIR}/bin/conn_rdma_base_unit_tests"
    # "${MCM_BUILD_DIR}/bin/conn_rdma_rx_tx_unit_tests"
fi
ln -sf "${MCM_BUILD_DIR}" "${SCRIPT_DIR}/build"

function print_success() { echo -e "\e[48;2;0;255;0;30m $* \e[0m"; }

print_success "Build Succeeded"
