# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: Copyright (c) 2023-2024 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

ARG IMAGE_CACHE_REGISTRY=docker.io
ARG IMAGE_NAME=library/ubuntu:22.04@sha256:a6d2b38300ce017add71440577d5b0a90460d0e57fd7aec21dd0d1b0761bbfb2

FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME} as builder

ARG PREFIX_DIR="/install"
ARG MCM_DIR="/opt/mcm"
ARG MTL_VER="9d87d0da3466bd88cdf3336844643882d272460b"
ARG MTL_DIR="/opt/mtl"
ARG DPDK_VER="23.11"
ARG DPDK_DIR="/opt/dpdk"
ARG XDP_DIR="/opt/xdp"
ARG GPRC_VER="v1.58.0"
ARG GRPC_DIR="/opt/grpc"
# JPEGXS_VER Release v0.9
ARG JPEGXS_VER="e1030c6c8ee2fb05b76c3fa14cccf8346db7a1fa"
ARG JPEGXS_DIR="/opt/jpegxs"

ENV DEBIAN_FRONTEND noninteractive
ENV TZ="Europe/Warsaw"
ENV PATH="${PREFIX_DIR}/bin:$HOME/.local/bin:$HOME/bin:$HOME/usr/bin:$PATH"

SHELL ["/bin/bash", "-ex", "-o", "pipefail", "-c"]
# Install package dependencies
RUN apt-get update --fix-missing && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        nasm cmake libbsd-dev git build-essential sudo \
        meson python3-dev python3-pyelftools pkg-config \
        libnuma-dev libjson-c-dev libpcap-dev libgtest-dev \
        libsdl2-dev libsdl2-ttf-dev libssl-dev ca-certificates \
        m4 clang llvm zlib1g-dev libelf-dev libcap-ng-dev \
        libcap2-bin gcc-multilib systemtap-sdt-dev librdmacm-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Clone MTL, DPDK and xdp-tools repo
WORKDIR ${DPDK_DIR}
RUN git config --global user.email "milosz.linkiewicz@intel.com" && \
    git config --global user.name "Milosz Linkiewicz" && \
    git clone --branch main https://github.com/OpenVisualCloud/Media-Transport-Library ${MTL_DIR} && \
    git -C ${MTL_DIR} checkout ${MTL_VER} && \
    git clone --branch v${DPDK_VER} --depth 1 https://github.com/DPDK/dpdk.git ${DPDK_DIR} && \
    git clone --recurse-submodules https://github.com/xdp-project/xdp-tools.git ${XDP_DIR} && \
    git clone --branch ${GPRC_VER} --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc ${GRPC_DIR} && \
    git clone https://github.com/OpenVisualCloud/SVT-JPEG-XS ${JPEGXS_DIR} && \
    git -C ${JPEGXS_DIR} checkout ${JPEGXS_VER} && \
    git am ${MTL_DIR}/patches/dpdk/${DPDK_VER}/*.patch

RUN meson setup build && \
    ninja -C build && \
    meson install -C build && \
    DESTDIR="${PREFIX_DIR}" meson install -C build

# Build the xdp-tools project
WORKDIR ${XDP_DIR}
RUN ./configure && \
    make && \
    make install && \
    DESTDIR="${PREFIX_DIR}" make install && \
    make -C "${XDP_DIR}/lib/libbpf/src" install && \
    DESTDIR="${PREFIX_DIR}" make -C "${XDP_DIR}/lib/libbpf/src" install

# Build MTL
WORKDIR ${MTL_DIR}
RUN ./build.sh && \
    DESTDIR="${PREFIX_DIR}" meson install -C build

# gRPC
WORKDIR ${GRPC_DIR}/cmake/build
RUN cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON ../.. && \
    make -j `nproc` && \
    make install && \
    cmake -DgRPC_BUILD_TESTS=ON -DgRPC_INSTALL=ON ../.. && \
    make -j `nproc` grpc_cli && \
    cp grpc_cli "${PREFIX_DIR}/usr/local/bin/"

# Build and install JPEG XS
WORKDIR ${JPEGXS_DIR}/Build/linux
RUN ./build.sh release install && \
    ./build.sh release --prefix "${PREFIX_DIR}/usr/local" install

# Build and install JPEG XS imtl plugin
WORKDIR ${JPEGXS_DIR}/imtl-plugin
RUN ./build.sh build-only && \
    DESTDIR="${PREFIX_DIR}" meson install --no-rebuild -C build && \
    mkdir -p /install/usr/local/etc/ && \
    cp ${JPEGXS_DIR}/imtl-plugin/kahawai.json /install/usr/local/etc/jpegxs.json

# Build MCM
WORKDIR ${MCM_DIR}
COPY . ${MCM_DIR}
RUN INSTALL_PREFIX="${PREFIX_DIR}/usr/local" ./build.sh && \
    setcap 'cap_ipc_lock+ep cap_ipc_owner+ep' /install/usr/local/bin/media_proxy && \
    setcap 'cap_net_bind_service+ep cap_net_broadcast+ep cap_net_raw+ep' /install/usr/local/bin/media_proxy && \
    rm -rf "${PREFIX_DIR}/usr/local/lib/lib"*.a && \
    rm -rf "${PREFIX_DIR}/usr/local/bin/dpdk-test"* && \
    rm -rf "${PREFIX_DIR}/usr/local/bin/"*_plugin && \
    rm -rf "${PREFIX_DIR}/usr/local/share"

ARG IMAGE_CACHE_REGISTRY
ARG IMAGE_NAME
FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME}

LABEL maintainer="milosz.linkiewicz@intel.com"
LABEL org.opencontainers.image.title="Media Communications Mesh Media Proxy"
LABEL org.opencontainers.image.description="Media Communications Mesh Media Proxy application. Ubuntu 22.04 Docker Container Release Image"
LABEL org.opencontainers.image.version="1.0.0"
LABEL org.opencontainers.image.vendor="Intel Corporation"
LABEL org.opencontainers.image.licenses="BSD 3-Clause License"

ARG MCM_DIR="/opt/mcm"
ARG GRPC_DIR="/opt/grpc"
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

ENV KAHAWAI_CFG_PATH="/usr/local/etc/jpegxs.json"
ENV LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib/x86_64-linux-gnu"

SHELL ["/bin/bash", "-exc"]
RUN apt-get update --fix-missing && \
    apt-get full-upgrade -y && \
    apt-get install -y --no-install-recommends \
       ca-certificates libbsd0 libnuma1 libjson-c5 libpcap0.8 libsdl2-2.0-0 libsdl2-ttf-2.0-0 \
       libssl3 zlib1g libelf1 libcap-ng0 libatomic1 librdmacm1 systemtap vim sudo \
       pciutils librte-net-mlx4-22 librte-net-mlx5-22 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd -g 2110 vfio && \
    groupadd -g 1001 imtl && \
    useradd -m -s /bin/bash -G vfio,imtl -u 1002 mcm && \
    usermod -aG sudo mcm

COPY --chown=mcm --from=builder /install /

RUN ldconfig

EXPOSE 8001 8002
CMD ["media_proxy"]

HEALTHCHECK --interval=30s --timeout=5s CMD ps aux | grep "media_proxy" || exit 1
