# SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

FROM ubuntu:22.04@sha256:8eab65df33a6de2844c9aefd19efe8ddb87b7df5e9185a4ab73af936225685bb as builder
LABEL maintainer="qiang.han@intel.com"

ENV DEBIAN_FRONTEND noninteractive

# Install package dependencies
RUN apt-get update --fix-missing \
    # && apt-get full-upgrade -y \
    && apt-get install --no-install-recommends -y apt-utils \
    software-properties-common ca-certificates \
    libtool autoconf automake pkg-config \
    sudo jq \
    python3 python3-pip \
    cmake \
    build-essential gcc meson \
    libnuma-dev libjson-c-dev libpcap-dev libgtest-dev \
    libsdl2-dev libsdl2-ttf-dev libssl-dev nlohmann-json3-dev \
    libbsd-dev \
    python3-pyelftools libgrpc++-dev protobuf-compiler \
    curl git git-lfs \
    vim openssh-server clang-format \
    gdb psmisc \
    pciutils iproute2 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ARG USER=docker
RUN useradd -l -m -s /bin/bash ${USER}

ARG PREFIX_DIR="/usr/local"
ARG HOME="/home/${USER}"

ENV PATH="${PREFIX_DIR}/bin:$HOME/.local/bin:$HOME/bin:$HOME/usr/bin:$PATH"

ARG MCM_DIR=/opt/mcm/media-communications-mesh
ARG MTL_DIR=/opt/mcm/3rd_party/mtl
ARG DPDK_DIR=/opt/mcm/3rd_party/dpdk

RUN mkdir -p /opt/mcm/3rd_party/
RUN adduser ${USER} sudo \
        && echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Install Conan using pip3 package installer.
RUN python3 -m pip install --no-cache-dir conan=1.62.0

# Build DPDK/IMTL
ENV DPDK_VER=23.03
RUN git clone --depth 1 --branch main https://github.com/OpenVisualCloud/Media-Transport-Library.git $MTL_DIR && \
    git clone --depth 1 --branch v${DPDK_VER} https://github.com/DPDK/dpdk.git $DPDK_DIR
WORKDIR $DPDK_DIR
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
RUN find $MTL_DIR/patches/dpdk/${DPDK_VER}/ -maxdepth 1 -name '*.patch' -print0 | sort -z | xargs -t -0 -n 1 patch -d ${DPDK_DIR} -p1 -i && \
    meson build && \
    ninja -C build && \
    ninja -C build install && \
    pkg-config --cflags libdpdk && \
    pkg-config --libs libdpdk && \
    pkg-config --modversion libdpdk

# Build IMTL
WORKDIR $MTL_DIR
RUN ./build.sh

# gRPC
ARG GPRC_VERSION="v1.58.0"
RUN git clone --recurse-submodules -b ${GPRC_VERSION} --depth 1 --shallow-submodules https://github.com/grpc/grpc /tmp/grpc \
    && mkdir -p /tmp/grpc/cmake/build
WORKDIR /tmp/grpc/cmake/build
RUN cmake -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX=${PREFIX_DIR} \
        ../.. && \
    make -j && \
    make install && \
    rm -rf /tmp/*

# Build MCM
RUN git clone --depth 1 --branch main https://github.com/OpenVisualCloud/Media-Communications-Mesh.git ${MCM_DIR}
WORKDIR $MCM_DIR
RUN ./build.sh

## Re-build container for optimised runtime environment using clean Ubuntu release
FROM ubuntu:22.04@sha256:8eab65df33a6de2844c9aefd19efe8ddb87b7df5e9185a4ab73af936225685bb

ARG USER=docker
RUN useradd -l -m -s /bin/bash ${USER}

RUN apt-get update --fix-missing \
&& apt-get install -y --no-install-recommends libnuma1 libjson-c5 libpcap0.8 libssl3 libatomic1 libbsd0 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/*

# Copy required files from build container
COPY --from=builder /usr/local /usr/local

RUN ldconfig

# Expose correct default ports to allow quick publishing
# EXPOSE 8001 8002

USER ${USER}

CMD ["media_proxy"]
