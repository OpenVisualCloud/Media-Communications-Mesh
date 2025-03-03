# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: Copyright (c) 2023-2024 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

ARG IMAGE_CACHE_REGISTRY=docker.io
ARG IMAGE_NAME=library/ubuntu:22.04@sha256:a6d2b38300ce017add71440577d5b0a90460d0e57fd7aec21dd0d1b0761bbfb2

FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME} AS builder

ARG VERSIONS_FILE="versions.env"
COPY "${VERSIONS_FILE}" /opt/versions.env
ENV BASH_ENV=/opt/versions.env

ARG MTL_VER=""
ARG DPDK_VER=""
ARG GPRC_VER=""
ARG JPEGXS_VER=""
ARG LIBFABRIC_VER=""

ARG MCM_DIR="/opt/mcm"
ARG MTL_DIR="/opt/mtl"
ARG DPDK_DIR="/opt/dpdk"
ARG XDP_DIR="/opt/xdp"
ARG GRPC_DIR="/opt/grpc"
ARG JPEGXS_DIR="/opt/jpegxs"
ARG LIBFABRIC_DIR="/opt/libfabric"

ARG GITHUB_CREDENTIALS=""
ARG PREFIX_DIR="/install"
ENV PATH="${PREFIX_DIR}/bin:/root/.local/bin:/root/bin:/root/usr/bin:$PATH"
ENV PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig

SHELL ["/bin/bash", "-ex", "-o", "pipefail", "-c"]
# Install package dependencies
RUN apt-get update --fix-missing && \
    apt-get full-upgrade -y && \
    apt-get install --no-install-recommends -y \
        curl nasm cmake libbsd-dev git build-essential sudo \
        meson python3-dev python3-pyelftools pkg-config \
        libnuma-dev libjson-c-dev libpcap-dev libgtest-dev \
        libsdl2-dev libsdl2-ttf-dev libssl-dev ca-certificates \
        m4 clang llvm zlib1g-dev libelf-dev libcap-ng-dev \
        gcc-multilib systemtap-sdt-dev librdmacm-dev \
        libfdt-dev autoconf automake autotools-dev libtool && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Install golang and dependencies
WORKDIR /usr/local/
RUN curl -Lf "https://go.dev/dl/go${GOLANG_GO_VER}.linux-amd64.tar.gz" -o "/usr/local/go${GOLANG_GO_VER}.linux-amd64.tar.gz" && \
    tar -C /usr/local -xzf /usr/local/go${GOLANG_GO_VER}.linux-amd64.tar.gz && \
    rm -f "/usr/local/go${GOLANG_GO_VER}.linux-amd64.tar.gz" && \
    ln -s /usr/local/go/bin/go /usr/bin/go && \
    go version && \
    go install "${GOLANG_PROTOBUF_GEN}" && \
    go install "${GOLANG_GRPC_GEN}"

# Clone MTL, DPDK and xdp-tools repo
WORKDIR ${DPDK_DIR}
RUN git clone --branch main https://${GITHUB_CREDENTIALS}github.com/OpenVisualCloud/Media-Transport-Library "${MTL_DIR}" && \
    git -C "${MTL_DIR}" checkout ${MTL_VER} && \
    git clone --branch v${DPDK_VER} --depth 1 https://github.com/DPDK/dpdk.git "${DPDK_DIR}" && \
    git clone --recurse-submodules https://github.com/xdp-project/xdp-tools.git "${XDP_DIR}" && \
    git clone --depth 1 --branch "${LIBFABRIC_VER}" https://github.com/ofiwg/libfabric "${LIBFABRIC_DIR}" && \
    git clone --branch ${GPRC_VER} --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc "${GRPC_DIR}" && \
    git clone https://github.com/OpenVisualCloud/SVT-JPEG-XS "${JPEGXS_DIR}" && \
    git -C ${JPEGXS_DIR} checkout ${JPEGXS_VER} && \
    patch -p1 -i <(cat "${MTL_DIR}/patches/dpdk/${DPDK_VER}/"*.patch)
RUN patch -d "${DPDK_DIR}" -p1 < "${MTL_DIR}/patches/dpdk/${DPDK_VER}/hdr_split/"*.patch || true
RUN patch -d "${DPDK_DIR}" -p1 < "${MTL_DIR}/patches/dpdk/${DPDK_VER}/tsn/"*.patch || true

# Build the xdp-tools project
WORKDIR ${XDP_DIR}
RUN ./configure && \
    make && \
    make install && \
    DESTDIR="${PREFIX_DIR}" make install && \
    make -C "${XDP_DIR}/lib/libbpf/src" && \
    make -C "${XDP_DIR}/lib/libbpf/src" install && \
    DESTDIR="${PREFIX_DIR}" make -C "${XDP_DIR}/lib/libbpf/src" install

WORKDIR ${LIBFABRIC_DIR}
RUN ./autogen.sh && \
    ./configure --enable-verbs && \
    make "-j$(nproc)" && \
    make install && \
    DESTDIR="${PREFIX_DIR}" make install

# Build the DPDK
WORKDIR ${DPDK_DIR}
RUN meson setup build && \
    ninja -C build && \
    meson install -C build && \
    DESTDIR="${PREFIX_DIR}" meson install -C build

# Build MTL
WORKDIR ${MTL_DIR}
RUN ./build.sh && \
    DESTDIR="${PREFIX_DIR}" meson install -C build

# gRPC
WORKDIR ${GRPC_DIR}/cmake/build
RUN cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON ../.. && \
    make "-j$(nproc)" && \
    make install && \
    cmake -DgRPC_BUILD_TESTS=ON -DgRPC_INSTALL=ON ../.. && \
    make "-j$(nproc)" grpc_cli && \
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
RUN ./build.sh "${PREFIX_DIR}" && \
    cp "${MTL_DIR}/build/manager/MtlManager" "${PREFIX_DIR}/usr/local/bin/" && \
    cp -fR "/usr/local/lib/bpf/"* "${PREFIX_DIR}/usr/local/lib/bpf/" && \
    rm -rf "${PREFIX_DIR}/usr/lib64/"*.a && \
    rm -rf "${PREFIX_DIR}/usr/include" && \
    rm -rf "${PREFIX_DIR}/usr/local/share" && \
    rm -rf "${PREFIX_DIR}/usr/local/lib/lib"*.a && \
    rm -rf "${PREFIX_DIR}/usr/local/lib/x86_64-linux-gnu/"*.a && \
    rm -rf "${PREFIX_DIR}/usr/local/bin/dpdk-"* && \
    rm -rf "${PREFIX_DIR}/usr/local/bin/grpc_cli" && \
    rm -rf "${PREFIX_DIR}/usr/local/bin/"*_plugin

ARG IMAGE_CACHE_REGISTRY
ARG IMAGE_NAME
FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME} as media-proxy

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
       libssl3 zlib1g libelf1 libcap-ng0 libatomic1 librdmacm1 systemtap sudo \
       librte-net-mlx4-22 librte-net-mlx5-22 libfdt1 ethtool && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd -g 2110 vfio && \
    groupadd -g 2001 mtl && \
    useradd -m -s /bin/bash -G vfio,mtl -u 2002 mcm && \
    usermod -aG sudo mcm

COPY --chown=mcm --from=builder /install /

RUN ldconfig && \
    apt-get purge -y '.*sound.*' '.*systemtap.*' '.*libdrm.*' '.*vorbis.*' '.*wayland.*' '.*x11.*' '.*python.*' '.*readline.*' '.*avahi.*' '^libx[a-w,y-z].*' make media-types libopus0 \
        libexpat1 libflac8 libfreetype6 libsndfile1 libgbm1 libglib2.0-0 libgraphite2-3 libharfbuzz0b libogg0 libpulse0 libmpdec3 libnspr4 libpng16-16 \
        xkb-data distro-info-data libdw1 libbrotli1 libdecor-0-0 libxxf86vm1 libapparmor1 libasyncns0 libnss3 libsqlite3-0 lsb-release
USER mcm
WORKDIR /opt/mcm/

EXPOSE 8001/tcp 8002/tcp

CMD ["--help"]
SHELL ["/bin/bash", "-c"]
ENTRYPOINT ["/usr/local/bin/media_proxy"]
HEALTHCHECK --interval=30s --timeout=5s CMD ps aux | grep "media_proxy" || exit 1

FROM media-proxy as mtl-manager

LABEL maintainer="milosz.linkiewicz@intel.com"
LABEL org.opencontainers.image.title="Media Communications Mesh Mtl Manager"
LABEL org.opencontainers.image.description="Media Communications Mesh Mtl Manager application. Ubuntu 22.04 Docker Container Release Image"
LABEL org.opencontainers.image.version="1.0.0"
LABEL org.opencontainers.image.vendor="Intel Corporation"
LABEL org.opencontainers.image.licenses="BSD 3-Clause License"

USER mtl
SHELL ["/bin/bash", "-c"]
ENTRYPOINT ["/usr/local/bin/MtlManager"]
HEALTHCHECK --interval=30s --timeout=5s CMD ps aux | grep "MtlManager" || exit 1

FROM media-proxy as mesh-agent

LABEL maintainer="milosz.linkiewicz@intel.com"
LABEL org.opencontainers.image.title="Media Communications Mesh Mesh Agent"
LABEL org.opencontainers.image.description="Media Communications Mesh Mesh Agent application. Ubuntu 22.04 Docker Container Release Image"
LABEL org.opencontainers.image.version="1.0.0"
LABEL org.opencontainers.image.vendor="Intel Corporation"
LABEL org.opencontainers.image.licenses="BSD 3-Clause License"

SHELL ["/bin/bash", "-c"]
ENTRYPOINT ["/usr/local/bin/mesh-agent"]
HEALTHCHECK --interval=30s --timeout=5s CMD ps aux | grep "mesh-agent" || exit 1
