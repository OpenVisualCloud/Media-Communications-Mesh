#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

# Passing custom path as a parameter to this script will install
# SDK in to default path and the one pointed as $1

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"

# shellcheck source="../scripts/common.sh"
. "${REPO_DIR}/scripts/common.sh"

# Set build type. ("Debug" or "Release")
MCM_SDK_DIR="${MCM_SDK_DIR:-${BUILD_DIR}/mcm-sdk}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -B "${MCM_SDK_DIR}" -S "${SCRIPT_DIR}"
make -j "${NPROC}" -C "${MCM_SDK_DIR}"

if [[ $# -ne 0 ]]; then
    DESTDIR="${1:-$DESTDIR}" make -C "${MCM_SDK_DIR}" install
fi
as_root make -C "${MCM_SDK_DIR}" install

ln -sf "${MCM_SDK_DIR}" "${SCRIPT_DIR}/build"
