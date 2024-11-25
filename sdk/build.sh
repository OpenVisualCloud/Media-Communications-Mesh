#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
BUILD_DIR="${BUILD_DIR:-${REPO_DIR}/_build}"
MCM_SDK_DIR="${MCM_SDK_DIR:-${BUILD_DIR}/mcm/sdk}"
# shellcheck source="../scripts/common.sh"
. "${SCRIPT_DIR}/../scripts/common.sh"

# Set build type. ("Debug" or "Release")
BUILD_TYPE="${BUILD_TYPE:-Release}"

cmake -B "${MCM_SDK_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${SCRIPT_DIR}"
cmake --build "${MCM_SDK_DIR}" -j

# Install
as_root cmake --install "${MCM_SDK_DIR}" --prefix "${INSTALL_PREFIX}"
