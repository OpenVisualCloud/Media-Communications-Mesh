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
mapfile -t BUILD_ARGUMENTS < <(compgen -e | sed -nE '/^(.*)(_proxy|_PROXY)$/{s/^/--build-arg=/;p}')
BUILD_ARGUMENTS+=( "--progress=plain" "--network=host" "--build-arg" "IMAGE_CACHE_REGISTRY=${IMAGE_CACHE_REGISTRY}" )

MCM_SDK="$(echo docker buildx build "${BUILD_ARGUMENTS[@]}" -t "${IMAGE_REGISTRY}/mcm/sdk:${IMAGE_TAG}" -f "${SCRIPT_DIR}/sdk/Dockerfile" "$@" "${SCRIPT_DIR}")"
MCM_MEDIA_PROXY="$(echo docker buildx build "${BUILD_ARGUMENTS[@]}" -t "${IMAGE_REGISTRY}/mcm/media-proxy:${IMAGE_TAG}" --target media-proxy -f "${SCRIPT_DIR}/media-proxy/Dockerfile" "$@" "${SCRIPT_DIR}")"
MCM_MTL_MANAGER="$(echo docker buildx build "${BUILD_ARGUMENTS[@]}" -t "${IMAGE_REGISTRY}/mcm/mtl-manager:${IMAGE_TAG}" --target mtl-manager -f "${SCRIPT_DIR}/media-proxy/Dockerfile" "$@" "${SCRIPT_DIR}")"
MCM_MESH_AGENT="$(echo docker buildx build "${BUILD_ARGUMENTS[@]}"  -t "${IMAGE_REGISTRY}/mcm/mesh-agent:${IMAGE_TAG}" --target mesh-agent  -f "${SCRIPT_DIR}/media-proxy/Dockerfile" "$@" "${SCRIPT_DIR}")"

FFMPEG_7_0="$(echo docker buildx build "${BUILD_ARGUMENTS[@]}" --build-arg FFMPEG_VER="7.0" -t "${IMAGE_REGISTRY}/mcm/ffmpeg:${IMAGE_TAG}" -f "${SCRIPT_DIR}/ffmpeg-plugin/Dockerfile" "$@" "${SCRIPT_DIR}")"
FFMPEG_6_1="$(echo docker buildx build "${BUILD_ARGUMENTS[@]}" --build-arg FFMPEG_VER="6.1" -t "${IMAGE_REGISTRY}/mcm/ffmpeg:6.1-${IMAGE_TAG}" -f "${SCRIPT_DIR}/ffmpeg-plugin/Dockerfile" "$@" "${SCRIPT_DIR}")"

export MCM_SDK
export MCM_MEDIA_PROXY
export MCM_MTL_MANAGER
export MCM_MESH_AGENT
export FFMPEG_7_0
export FFMPEG_6_1

function build_and_tag_all_dockerfiles() {
    $MCM_SDK
    $MCM_MEDIA_PROXY
    $MCM_MTL_MANAGER
    $MCM_MESH_AGENT
    $FFMPEG_7_0
    $FFMPEG_6_1

    docker tag "${IMAGE_REGISTRY}/mcm/sdk:${IMAGE_TAG}"         "mcm/sample-app:${IMAGE_TAG}"
    docker tag "${IMAGE_REGISTRY}/mcm/media-proxy:${IMAGE_TAG}" "mcm/media-proxy:${IMAGE_TAG}"
    docker tag "${IMAGE_REGISTRY}/mcm/mtl-manager:${IMAGE_TAG}" "mcm/mtl-manager:${IMAGE_TAG}"
    docker tag "${IMAGE_REGISTRY}/mcm/mesh-agent:${IMAGE_TAG}"  "mcm/mesh-agent:${IMAGE_TAG}"
    docker tag "${IMAGE_REGISTRY}/mcm/ffmpeg:${IMAGE_TAG}"      "mcm/ffmpeg:${IMAGE_TAG}"
    docker tag "${IMAGE_REGISTRY}/mcm/ffmpeg:6.1-${IMAGE_TAG}"  "mcm/ffmpeg:6.1-${IMAGE_TAG}"
}
function print_build_help_dockerfiles() {
    echo -e MCM_SDK="${MCM_SDK}\n"
    echo -e MCM_MEDIA_PROXY="${MCM_MEDIA_PROXY}\n"
    echo -e MCM_MTL_MANAGER="${MCM_MTL_MANAGER}\n"
    echo -e MCM_MESH_AGENT="${MCM_MESH_AGENT}\n"
    echo -e FFMPEG_7_0="${FFMPEG_7_0}\n"
    echo -e FFMPEG_6_1="${FFMPEG_6_1}\n"
}

# Allow sourcing of the script. Run only when asked
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
# ======= RUNTIME EXEC START
    build_and_tag_all_dockerfiles
    print_build_help_dockerfiles
# ======= RUNTIME EXEC STOP
else
# ======= SOURCED SCRIPT START
    return 0
# ======= RUNTIME SCRIPT STOP
fi
