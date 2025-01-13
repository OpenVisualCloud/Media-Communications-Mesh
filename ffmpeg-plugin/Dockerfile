# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: Copyright (c) 2023-2024 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

ARG IMAGE_CACHE_REGISTRY=docker.io
ARG IMAGE_NAME=library/ubuntu:22.04@sha256:a6d2b38300ce017add71440577d5b0a90460d0e57fd7aec21dd0d1b0761bbfb2

FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME} AS builder

ARG VERSIONS_FILE="versions.env"
COPY "${VERSIONS_FILE}" /opt/versions.env
ENV BASH_ENV=/opt/versions.env

ARG GPRC_VER=""
ARG FFMPEG_VER=""
ARG MCM_DIR=/opt/mcm
ARG PREFIX_DIR="/install"
ARG GRPC_DIR="/opt/grpc"

SHELL ["/bin/bash", "-exo", "pipefail", "-c"]
RUN apt-get update && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        build-essential \
        ca-certificates \
        libbsd-dev \
        libx264-dev \
        pkg-config \
        apt-utils \
        sudo \
        make \
        cmake \
        git \
        nasm \
        curl \
        tar && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    mkdir -p "${GRPC_DIR}" && \
    git clone --branch ${GPRC_VER} --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc "${GRPC_DIR}"

# gRPC
WORKDIR ${GRPC_DIR}/cmake/build
RUN cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON ../.. && \
    make "-j$(nproc)" && \
    make install

COPY . ${MCM_DIR}
WORKDIR ${MCM_DIR}
RUN ./sdk/build.sh && \
    INSTALL_PREFIX="${PREFIX_DIR}/usr/local" ./sdk/build.sh && \
    ./ffmpeg-plugin/clone-and-patch-ffmpeg.sh "${FFMPEG_VER}" && \
    ./ffmpeg-plugin/configure-ffmpeg.sh "${FFMPEG_VER}" --enable-libx264 --enable-gpl --prefix="${PREFIX_DIR}/usr/local"

WORKDIR ${MCM_DIR}/ffmpeg-plugin
RUN ./build-ffmpeg.sh "${FFMPEG_VER}"

ARG IMAGE_CACHE_REGISTRY
ARG IMAGE_NAME
FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME}

LABEL maintainer="milosz.linkiewicz@intel.com"
LABEL org.opencontainers.image.title="Media Communications Mesh FFmpeg"
LABEL org.opencontainers.image.description="Media Communications Mesh FFmpeg with MCM-Muxer and MCM-Demuxer plugins. Ubuntu 22.04 Docker Release Image."
LABEL org.opencontainers.image.version="1.0.0"
LABEL org.opencontainers.image.vendor="Intel Corporation"
LABEL org.opencontainers.image.licenses="BSD 3-Clause License"

ARG MCM_DIR=/opt/mcm
ARG PREFIX_DIR="/install"

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw
ENV HOME="${MCM_DIR}"

SHELL ["/bin/bash", "-exc"]
RUN apt-get update && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        ca-certificates \
        vim net-tools \
        libx264-163 \
        libbsd0 \
        sudo && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd -g 2110 vfio && \
    groupadd -g 1001 imtl && \
    useradd -m -s /bin/bash -G vfio,imtl -u 1002 mcm && \
    usermod -aG sudo mcm

COPY --chown=mcm --from=builder "${PREFIX_DIR}" /
RUN ldconfig

USER mcm
WORKDIR /opt/mcm/
EXPOSE 8001/tcp 8002/tcp

CMD ["--help"]
SHELL ["/bin/bash", "-c"]
ENTRYPOINT ["/usr/local/bin/ffmpeg"]

HEALTHCHECK --interval=30s --timeout=5s CMD ps aux | grep "ffmpeg" || exit 1
