#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
. "${SCRIPT_DIR}/common.sh"

# Read proxy variables from env to pass them to the builder
BUILD_ARGUMENTS=$(compgen -e | sed -nE '/_(proxy|PROXY)$/{s/^/--build-arg /;p}')

IMAGE_CACHE_REGISTRY="${IMAGE_CACHE_REGISTRY:-docker.io}"
IMAGE_REGISTRY="${IMAGE_REGISTRY:-docker.io}"
IMAGE_TAG="${IMAGE_TAG:-latest}"

docker buildx build --progress=plain --network=host ${BUILD_ARGUMENTS} --build-arg IMAGE_CACHE_REGISTRY="${IMAGE_CACHE_REGISTRY}" -t "${IMAGE_REGISTRY}/ffmpeg:${IMAGE_TAG}" -f "${SCRIPT_DIR}/ffmpeg-plugin/Dockerfile" $@ "${SCRIPT_DIR}"
docker buildx build --progress=plain --network=host ${BUILD_ARGUMENTS} --build-arg IMAGE_CACHE_REGISTRY="${IMAGE_CACHE_REGISTRY}" -t "${IMAGE_REGISTRY}/sdk:${IMAGE_TAG}" -f "${SCRIPT_DIR}/sdk/Dockerfile" $@ "${SCRIPT_DIR}"
docker buildx build --progress=plain --network=host ${BUILD_ARGUMENTS} --build-arg IMAGE_CACHE_REGISTRY="${IMAGE_CACHE_REGISTRY}" -t "${IMAGE_REGISTRY}/media-proxy:${IMAGE_TAG}" -f "${SCRIPT_DIR}/media-proxy/Dockerfile" $@ "${SCRIPT_DIR}"
