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

ENV MCM_USER=mcm \
    MCM_GROUP=mcm \
    MCM_UID=994 \
    MCM_GID=994

ENV KAHAWAI_CFG_PATH="/usr/local/etc/jpegxs.json"
ENV LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib/x86_64-linux-gnu"

WORKDIR /opt/mcm/
SHELL ["/bin/bash", "-exc"]
RUN apt-get update --fix-missing && \
    apt-get full-upgrade -y && \
    apt-get install -y --no-install-recommends \
       ca-certificates libbsd0 libnuma1 libjson-c5 libpcap0.8 libsdl2-2.0-0 libsdl2-ttf-2.0-0 \
       libssl3 zlib1g libelf1 libcap-ng0 libatomic1 librdmacm1 systemtap sudo \
       librte-net-mlx4-22 librte-net-mlx5-22 libfdt1 ethtool libcap2-bin && \
    apt-get purge -y '*sound*' '*systemtap*' '*libdrm*' '*vorbis*' '*wayland*' '*x11*' '*python*' '*readline*' '*avahi*' '^libx[a-w,y-z].*' make media-types libopus0 \
        libexpat1 libflac8 libfreetype6 libsndfile1 libgbm1 libglib2.0-0 libgraphite2-3 libharfbuzz0b libogg0 libpulse0 libmpdec3 libnspr4 libpng16-16 \
        xkb-data distro-info-data libdw1 libbrotli1 libdecor-0-0 libxxf86vm1 libapparmor1 libasyncns0 libnss3 libsqlite3-0 lsb-release && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --chown=${MCM_USER}:${MCM_GROUP} --from=builder /install /
RUN ldconfig && \
    mkdir /opt/grpc && \
    groupadd -g 2110 vfio && \
    groupadd -g 993 imtl && \
    getent group ${MCM_GROUP} > /dev/null || groupadd -r ${MCM_GROUP} -g "${MCM_GID}" && \
    getent passwd ${MCM_USER} > /dev/null || useradd -r -g ${MCM_GROUP} -u "${MCM_UID}" -s /sbin/nologin ${MCM_USER} -d /opt/mcm && \
    usermod -aG sudo,vfio,imtl ${MCM_USER} && \
    chown -R ${MCM_USER}:${MCM_GROUP} /opt/mcm /opt/grpc && \
    chown -R root:${MCM_GROUP} /usr/lib64 /usr/local /run /var/run && \
    chmod -R 2775 /usr/lib64 /usr/local /run /var/run /opt/mcm  && \
    passwd -d ${MCM_USER} && \
    ln -s /usr/lib64/libbpf.so.1 /usr/lib/x86_64-linux-gnu/libbpf.so.1 && \
    setcap "cap_sys_nice,cap_bpf,cap_dac_override,cap_ipc_lock,cap_setgid,cap_sys_rawio,cap_net_raw,cap_sys_admin,cap_net_admin+eip" /usr/local/bin/media_proxy
    # add docker flags --cap-add sys_nice --cap-add bpf --cap-add dac_override --cap-add ipc_lock --cap-add setgid --cap-add sys_rawio --cap-add net_raw --cap-add sys_admin --cap-add net_admin

USER mcm
EXPOSE 8001/tcp 8002/tcp

CMD ["--help"]
SHELL ["/bin/bash", "-c"]
ENTRYPOINT ["/usr/local/bin/media_proxy"]

HEALTHCHECK --interval=30s --timeout=5s CMD ps aux | grep "media_proxy" || exit 1
