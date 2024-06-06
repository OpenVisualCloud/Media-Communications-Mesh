#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"

# Read proxy variables from env to pass them to the builder
BUILD_ARGUMENTS=$(compgen -e | sed -nE '/_(proxy|PROXY)$/{s/^/--build-arg /;p}')

docker buildx build --progress=plain --network=host ${BUILD_ARGUMENTS} -f "${SCRIPT_DIR}/ffmpeg-plugin/Dockerfile" $@ "${SCRIPT_DIR}"
