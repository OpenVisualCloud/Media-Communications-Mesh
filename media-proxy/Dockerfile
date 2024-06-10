# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: Copyright (c) 2023-2024 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

ARG IMAGE_CACHE_REGISTRY=docker.io
ARG IMAGE_NAME=library/ubuntu:22.04@sha256:a6d2b38300ce017add71440577d5b0a90460d0e57fd7aec21dd0d1b0761bbfb2

FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME} as builder

ARG PREFIX_DIR="/usr/local"
# ARG PREFIX_DIR="/opt/install"
ARG DPDK_VER="23.11"
ARG GPRC_VER="v1.58.0"
ARG MCM_DIR="/opt/mcm"
ARG MTL_DIR="/opt/mtl"
ARG DPDK_DIR="/opt/dpdk"
ARG XDP_DIR="/opt/xdp"
ARG GRPC_DIR="/opt/grpc"

ENV DEBIAN_FRONTEND noninteractive
ENV TZ="Europe/Warsaw"
ENV PATH="${PREFIX_DIR}/bin:$HOME/.local/bin:$HOME/bin:$HOME/usr/bin:$PATH"

SHELL ["/bin/bash", "-ex", "-o", "pipefail", "-c"]
# Install package dependencies
RUN apt-get update --fix-missing && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        sudo nasm cmake libbsd-dev git build-essential \
        meson python3 python3-pyelftools pkg-config \
        libnuma-dev libjson-c-dev libpcap-dev libgtest-dev \
        libsdl2-dev libsdl2-ttf-dev libssl-dev ca-certificates \
        m4 clang llvm zlib1g-dev libelf-dev libcap-ng-dev \
        libcap2-bin gcc-multilib systemtap-sdt-dev librdmacm-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Clone MTL, DPDK and xdp-tools repo
WORKDIR ${DPDK_DIR}
RUN git config --global user.email "you@intel.com" && \
    git config --global user.name "Your Name" && \
    git clone --branch main https://github.com/OpenVisualCloud/Media-Transport-Library ${MTL_DIR} && \
    git clone --branch v${DPDK_VER} https://github.com/DPDK/dpdk.git ${DPDK_DIR} && \
    git clone --recurse-submodules https://github.com/xdp-project/xdp-tools.git ${XDP_DIR} && \
    git clone --branch ${GPRC_VER} --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc ${GRPC_DIR} && \
    git am ${MTL_DIR}/patches/dpdk/${DPDK_VER}/*.patch && \
    meson setup build && \
    meson install -C build && \
    DESTDIR="${PREFIX_DIR}" meson install -C build && \
    pkg-config --cflags libdpdk && \
    pkg-config --libs libdpdk && \
    pkg-config --modversion libdpdk

# Build the xdp-tools project
WORKDIR ${XDP_DIR}
RUN ./configure && \
    make && \
    make install && \
    DESTDIR="${PREFIX_DIR}" make install
WORKDIR ${XDP_DIR}/lib/libbpf/src
RUN make install && \
    DESTDIR="${PREFIX_DIR}" make install

# Build IMTL
WORKDIR ${MTL_DIR}
RUN ./build.sh && \
    DESTDIR="${PREFIX_DIR}" meson install -C build

# gRPC
WORKDIR ${GRPC_DIR}/cmake/build
RUN cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON \
        -DCMAKE_INSTALL_PREFIX="${PREFIX_DIR}" ../.. && \
    make -j `nproc` && \
    make install && \
    cmake -DgRPC_BUILD_TESTS=ON -DgRPC_INSTALL=ON \
        -DCMAKE_INSTALL_PREFIX="${PREFIX_DIR}" ../.. && \
    make -j `nproc` grpc_cli

# Build MCM
WORKDIR ${MCM_DIR}
COPY . ${MCM_DIR}
RUN CMAKE_PREFIX_PATH="${PREFIX_DIR}" INSTALL_PREFIX="${PREFIX_DIR}" ./build.sh

ARG IMAGE_CACHE_REGISTRY
ARG IMAGE_NAME
FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME}

LABEL maintainer="milosz.linkiewicz@intel.com"
LABEL org.opencontainers.image.title="Intel® Media Communication Mesh Media Proxy" \
      org.opencontainers.image.description="Intel® Media Communications Mesh Media Proxy application. Ubuntu 22.04 Docker Container Release Image" \
      org.opencontainers.image.version="0.5.0" \
      org.opencontainers.image.vendor="Intel Corporation" \
      org.opencontainers.image.licenses="BSD 3-Clause License"

ARG MCM_DIR="/opt/mcm"
ARG GRPC_DIR="/opt/grpc"
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

ENV KAHAWAI_CFG_PATH="/usr/local/etc/imtl.json"
ENV LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib/x86_64-linux-gnu"

SHELL ["/bin/bash", "-exc"]
RUN apt-get update --fix-missing && \
    apt-get full-upgrade -y && \
    apt-get install -y --no-install-recommends \
       apt-transport-https software-properties-common ca-certificates \
       libbsd0 libnuma1 libjson-c5 libpcap0.8 libsdl2-2.0-0 libsdl2-ttf-2.0-0 libssl3 \
       zlib1g libelf1 libcap-ng0 libatomic1 librdmacm1 systemtap \
       vim net-tools sudo  && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd -g 2110 vfio && \
    useradd -m -s /bin/bash -G vfio -u 1002 mcm && \
    usermod -aG sudo mcm

COPY --from=builder /usr/local /usr/local
COPY --from=builder /usr/lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu
COPY --from=builder /usr/lib64 /usr/lib64

RUN ldconfig
EXPOSE 8001 8002
CMD ["media_proxy"]

HEALTHCHECK --interval=30s --timeout=5s CMD ps aux | grep "media_proxy" || exit 1
