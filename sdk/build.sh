#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

# Passing custom path as a parameter to this script will install
# SDK in to default path and the one pointed as $1

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
BUILD_DIR="${BUILD_DIR:-${REPO_DIR}/_build}"
MCM_SDK_DIR="${MCM_SDK_DIR:-${BUILD_DIR}/mcm-sdk}"

# shellcheck source="../scripts/common.sh"
. "${SCRIPT_DIR}/../scripts/common.sh"

# Set build type. ("Debug" or "Release")
BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -B "${MCM_SDK_DIR}" -S "${SCRIPT_DIR}"

as_root make -C "${MCM_SDK_DIR}" install

if [[ $# -ne 0 ]]; then
    DESTDIR="${1:-$DESTDIR}" make -C "${MCM_SDK_DIR}" install
fi
