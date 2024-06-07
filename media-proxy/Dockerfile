
# SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

FROM ubuntu:24.04@sha256:e3f92abc0967a6c19d0dfa2d55838833e947b9d74edbcb0113e48535ad4be12a as builder
LABEL maintainer="qiang.han@intel.com"

ARG USER=ubuntu
ARG UID=1000

ENV DEBIAN_FRONTEND noninteractive

SHELL ["/bin/bash", "-ex", "-o", "pipefail", "-c"]
# Install package dependencies
RUN apt-get update --fix-missing && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y apt-utils \
    software-properties-common ca-certificates \
    libtool autoconf automake pkg-config \
    sudo jq \
    python3 python3-pip \
    cmake nasm \
    build-essential gcc meson \
    libnuma-dev libjson-c-dev libpcap-dev libgtest-dev \
    libsdl2-dev libsdl2-ttf-dev libssl-dev nlohmann-json3-dev \
    libbsd-dev \
    python3-pyelftools libgrpc++-dev protobuf-compiler \
    curl git git-lfs \
    vim openssh-server clang-format \
    gdb psmisc && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    useradd -l -m -s /bin/bash -u ${UID} ${USER}

ARG PREFIX_DIR="/usr/local"
ARG HOME="/home/${USER}"

ENV PATH="${PREFIX_DIR}/bin:$HOME/.local/bin:$HOME/bin:$HOME/usr/bin:$PATH"

ARG MCM_DIR=/opt/mcm
ARG MTL_DIR=/opt/mtl
ARG DPDK_DIR=/opt/dpdk
ENV DPDK_VER=23.11

# Build DPDK/IMTL
WORKDIR ${DPDK_DIR}
RUN git clone --depth 1 --branch main https://github.com/OpenVisualCloud/Media-Transport-Library.git ${MTL_DIR} && \
    git clone --depth 1 --branch v${DPDK_VER} https://github.com/DPDK/dpdk.git . && \
    find ${MTL_DIR}/patches/dpdk/${DPDK_VER}/ -maxdepth 1 -name '*.patch' -print0 | sort -z | xargs -t -0 -n 1 patch -d ${DPDK_DIR} -p1 -i && \
    meson build && \
    ninja -C build && \
    ninja -C build install && \
    pkg-config --cflags libdpdk && \
    pkg-config --libs libdpdk && \
    pkg-config --modversion libdpdk

# Build IMTL
WORKDIR ${MTL_DIR}
RUN ./build.sh

# gRPC
ARG GPRC_VERSION="v1.58.0"
RUN git clone --recurse-submodules -b ${GPRC_VERSION} --depth 1 --shallow-submodules https://github.com/grpc/grpc /tmp/grpc
WORKDIR /tmp/grpc/cmake/build
RUN cmake -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX=${PREFIX_DIR} \
        ../.. && \
    make -j `nproc` && \
    make install && \
    rm -rf /tmp/*

# Build MCM
RUN git clone --depth 1 --branch main https://github.com/OpenVisualCloud/Media-Communications-Mesh.git ${MCM_DIR}
WORKDIR $MCM_DIR
RUN ./build.sh

# Re-build container for optimised runtime environment using clean Ubuntu release
FROM ubuntu:24.04@sha256:e3f92abc0967a6c19d0dfa2d55838833e947b9d74edbcb0113e48535ad4be12a
LABEL maintainer="qiang.han@intel.com"

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

ARG USER=ubuntu
ARG UID=1000
# Set default environment variables for the user
ARG PREFIX_DIR="/usr/local"
SHELL ["/bin/bash", "-c"]
RUN apt-get update --fix-missing && \
    apt-get full-upgrade -y && \
    apt-get install -y --no-install-recommends \
        libsdl2-2.0-0 libsdl2-ttf-2.0-0 libnuma1 libjson-c5 libpcap0.8 libssl3 libatomic1 libbsd0 \
        sleepenh net-tools sudo jq && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    useradd -l -m -s /bin/bash -u ${UID} ${USER}


RUN ldconfig

# Expose correct default ports to allow quick publishing
# EXPOSE 8001 8002

USER ${USER}

CMD ["media_proxy"]

# Add a health check command to check if a specific program is running
HEALTHCHECK --interval=30s --timeout=3s \
  CMD ps aux | grep "media_proxy" || exit 1
