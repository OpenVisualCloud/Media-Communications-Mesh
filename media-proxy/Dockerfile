# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: Copyright (c) 2023-2024 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

ARG IMAGE_CACHE_REGISTRY=docker.io
ARG IMAGE_NAME=library/ubuntu:22.04@sha256:a6d2b38300ce017add71440577d5b0a90460d0e57fd7aec21dd0d1b0761bbfb2

FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME} AS builder

ARG MCM_DIR="/opt/mcm"
ARG VERSIONS_FILE="versions.env"
ARG GITHUB_CREDENTIALS=""
COPY . "${MCM_DIR}"
COPY "${VERSIONS_FILE}" "${MCM_DIR}/versions.env"

SHELL ["/bin/bash", "-ex", "-o", "pipefail", "-c"]
ENV BASH_ENV="${MCM_DIR}/versions.env"

ENV PREFIX_DIR="/install"
ENV REPO_DIR="${MCM_DIR}"
ENV BUILD_DIR="${REPO_DIR}/_build}"
ENV GRPC_DIR="${BUILD_DIR}/grpc"
ENV DPDK_DIR="${BUILD_DIR}/dpdk"
ENV XDP_DIR="${BUILD_DIR}/xdp"
ENV JPEGXS_DIR="${BUILD_DIR}/jpegxs"
ENV LIBFABRIC_DIR="${BUILD_DIR}/libfabric"

ENV PATH="${PREFIX_DIR}/bin:/root/.local/bin:/root/bin:/root/usr/bin:$PATH"
ENV PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig

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

WORKDIR "${MCM_DIR}"
RUN scripts/setup_build_env.sh

# Build MCM
WORKDIR ${MCM_DIR}
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
FROM ${IMAGE_CACHE_REGISTRY}/${IMAGE_NAME} AS media-proxy

LABEL maintainer="milosz.linkiewicz@intel.com,konstantin.ilichev@intel.com"
LABEL org.opencontainers.image.title="Media Communications Mesh Media Proxy"
LABEL org.opencontainers.image.description="Media Communications Mesh Media Proxy application. Ubuntu 22.04 Docker Container Release Image"
LABEL org.opencontainers.image.version="25.03.0"
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

FROM media-proxy AS mtl-manager

LABEL maintainer="milosz.linkiewicz@intel.com,konstantin.ilichev@intel.com"
LABEL org.opencontainers.image.title="Media Communications Mesh Mtl Manager"
LABEL org.opencontainers.image.description="Media Communications Mesh Mtl Manager application. Ubuntu 22.04 Docker Container Release Image"
LABEL org.opencontainers.image.version="25.03.0"
LABEL org.opencontainers.image.vendor="Intel Corporation"
LABEL org.opencontainers.image.licenses="BSD 3-Clause License"

USER mtl
SHELL ["/bin/bash", "-c"]
ENTRYPOINT ["/usr/local/bin/MtlManager"]
HEALTHCHECK --interval=30s --timeout=5s CMD if(( $(ps aux | grep "MtlManager" | wc -l) != 2 )); then exit 1; fi

FROM media-proxy AS mesh-agent

LABEL maintainer="milosz.linkiewicz@intel.com,konstantin.ilichev@intel.com"
LABEL org.opencontainers.image.title="Media Communications Mesh Mesh Agent"
LABEL org.opencontainers.image.description="Media Communications Mesh Mesh Agent application. Ubuntu 22.04 Docker Container Release Image"
LABEL org.opencontainers.image.version="25.03.0"
LABEL org.opencontainers.image.vendor="Intel Corporation"
LABEL org.opencontainers.image.licenses="BSD 3-Clause License"

USER mcm
WORKDIR /opt/mcm/

EXPOSE 8100/tcp 50051/tcp

SHELL ["/bin/bash", "-c"]
ENTRYPOINT ["/usr/local/bin/mesh-agent"]

HEALTHCHECK --interval=30s --timeout=5s CMD if(( $(ps aux | grep "mesh-agent" | wc -l) != 2 )); then exit 1; fi
