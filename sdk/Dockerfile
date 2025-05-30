# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

ARG IMAGE_CACHE_REGISTRY=docker.io
ARG IMAGE_NAME=library/ubuntu:22.04@sha256:a6d2b38300ce017add71440577d5b0a90460d0e57fd7aec21dd0d1b0761bbfb2

FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME} AS builder

ARG VERSIONS_FILE="versions.env"
COPY "${VERSIONS_FILE}" /opt/versions.env
ENV BASH_ENV=/opt/versions.env

ARG MCM_DIR=/opt/mcm
ARG PREFIX_DIR="/install"
ARG GRPC_DIR="/opt/grpc"

SHELL ["/bin/bash", "-exo", "pipefail", "-c"]
RUN apt-get update && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        apt-utils build-essential make cmake git ca-certificates pkg-config nasm libbsd-dev && \
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

WORKDIR ${MCM_DIR}/sdk
RUN ./build.sh "${PREFIX_DIR}"

ARG IMAGE_CACHE_REGISTRY
ARG IMAGE_NAME
FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME}

LABEL maintainer="milosz.linkiewicz@intel.com,konstantin.ilichev@intel.com"
LABEL org.opencontainers.image.title="Media Communications Mesh SDK"
LABEL org.opencontainers.image.description="Media Communications Mesh SDK sample applications. Ubuntu 22.04 Docker Release Image"
LABEL org.opencontainers.image.version="25.03.0"
LABEL org.opencontainers.image.vendor="Intel Corporation"
LABEL org.opencontainers.image.licenses="BSD 3-Clause License"

ARG MCM_DIR="/opt/mcm"
ARG PREFIX_DIR="/install"

ENV HOME="${MCM_DIR}"
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw
ENV PATH="/usr/local/bin:/home/mcm/.local/bin:/root/.local/bin:$PATH"
ENV LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib/x86_64-linux-gnu"

SHELL ["/bin/bash", "-exc"]
WORKDIR "/opt/mcm/"
RUN apt-get update && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        libbsd0 vim net-tools ca-certificates sudo && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd -g 2110 vfio && \
    groupadd -g 1001 imtl && \
    useradd -m -s /bin/bash -G vfio,imtl -u 1002 mcm && \
    usermod -aG sudo mcm

COPY --chown=1002 --from=builder "${PREFIX_DIR}" /

RUN ldconfig
WORKDIR /opt/mcm/
EXPOSE 8001/tcp 8002/tcp

USER mcm

CMD ["--help"]
SHELL ["/bin/bash", "-c"]
ENTRYPOINT ["${MCM_DIR}/recver_app"]
HEALTHCHECK --interval=30s --timeout=5s CMD if(( $(ps aux | grep "recver_app\|sender_app" | wc -l) != 2 )); then exit 1; fi
