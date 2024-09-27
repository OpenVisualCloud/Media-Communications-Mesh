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

docker "${BASIC_ARGUMENTS[@]}" "${BUILD_ARGUMENTS[@]}" -t "${IMAGE_REGISTRY}/mcm/sdk:${IMAGE_TAG}" -f "${SCRIPT_DIR}/sdk/Dockerfile" "$@" "${SCRIPT_DIR}"
docker "${BASIC_ARGUMENTS[@]}" "${BUILD_ARGUMENTS[@]}" -t "${IMAGE_REGISTRY}/mcm/media-proxy:${IMAGE_TAG}" -f "${SCRIPT_DIR}/media-proxy/Dockerfile" "$@" "${SCRIPT_DIR}"
docker "${BASIC_ARGUMENTS[@]}" "${BUILD_ARGUMENTS[@]}" -t "${IMAGE_REGISTRY}/mcm/ffmpeg:7.0-${IMAGE_TAG}" -f "${SCRIPT_DIR}/ffmpeg-plugin/Dockerfile" --build-arg FFMPEG_VER="7.0" "$@" "${SCRIPT_DIR}"
docker "${BASIC_ARGUMENTS[@]}"  "${BUILD_ARGUMENTS[@]}" -t "${IMAGE_REGISTRY}/mcm/ffmpeg:6.1-${IMAGE_TAG}" -f "${SCRIPT_DIR}/ffmpeg-plugin/Dockerfile" --build-arg FFMPEG_VER="6.1" "$@" "${SCRIPT_DIR}"

docker tag "${IMAGE_REGISTRY}/mcm/sdk:${IMAGE_TAG}" "mcm/sample-app:${IMAGE_TAG}"
docker tag "${IMAGE_REGISTRY}/mcm/media-proxy:${IMAGE_TAG}" "mcm/media-proxy:${IMAGE_TAG}"
docker tag "${IMAGE_REGISTRY}/mcm/ffmpeg:7.0-${IMAGE_TAG}" "mcm/ffmpeg:${IMAGE_TAG}"
docker tag "${IMAGE_REGISTRY}/mcm/ffmpeg:7.0-${IMAGE_TAG}" "mcm/ffmpeg:7.0-${IMAGE_TAG}"
docker tag "${IMAGE_REGISTRY}/mcm/ffmpeg:6.1-${IMAGE_TAG}" "mcm/ffmpeg:6.1-${IMAGE_TAG}"

docker push "${IMAGE_REGISTRY}/mcm/sdk:${IMAGE_TAG}"
docker push "${IMAGE_REGISTRY}/mcm/media-proxy:${IMAGE_TAG}"
docker push "${IMAGE_REGISTRY}/mcm/ffmpeg:7.0-${IMAGE_TAG}"
docker push "${IMAGE_REGISTRY}/mcm/ffmpeg:6.1-${IMAGE_TAG}"
