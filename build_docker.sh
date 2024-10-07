#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
. "${SCRIPT_DIR}/scripts/common.sh"

IMAGE_CACHE_REGISTRY="${IMAGE_CACHE_REGISTRY:-docker.io}"
IMAGE_REGISTRY="${IMAGE_REGISTRY:-docker.io}"
IMAGE_TAG="${IMAGE_TAG:-latest}"

# Read proxy variables from env to pass them to the builder
BUILD_ARGUMENTS=()
BASIC_ARGUMENTS=("buildx" "build" "--progress=plain" "--network=host" "--build-arg=IMAGE_CACHE_REGISTRY=${IMAGE_CACHE_REGISTRY}")
mapfile -t BUILD_ARGUMENTS < <(compgen -e | sed -nE '/^(.*)(_proxy|_PROXY)$/{s/^/--build-arg=/;p}')


docker tag "${IMAGE_REGISTRY}/sdk:${IMAGE_TAG}" "mcm/sample-app:${IMAGE_TAG}"
docker tag "${IMAGE_REGISTRY}/media-proxy:${IMAGE_TAG}" "mcm/media-proxy:${IMAGE_TAG}"
docker tag "${IMAGE_REGISTRY}/ffmpeg:${IMAGE_TAG}" "mcm/ffmpeg:${IMAGE_TAG}"
