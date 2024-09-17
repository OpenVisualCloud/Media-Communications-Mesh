#!/bin/bash

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="${SCRIPT_DIR}"

. "${REPO_DIR}/common.sh"

export MCM_DIR="${REPO_DIR}/build/mcm"
export MTL_VER="9d87d0da3466bd88cdf3336844643882d272460b"
export MTL_DIR="${REPO_DIR}/build/mtl"
export DPDK_VER="23.11"
export DPDK_DIR="${REPO_DIR}/build/dpdk"
export XDP_DIR="${REPO_DIR}/build/xdp"
export GPRC_VER="v1.58.0"
export GRPC_DIR="${REPO_DIR}/build/grpc"
export JPEGXS_VER="e1030c6c8ee2fb05b76c3fa14cccf8346db7a1fa"
export JPEGXS_DIR="${REPO_DIR}/build/jpegxs"
export LIBFABRIC_DIR="${REPO_DIR}/build/libfabric"

export DEBIAN_FRONTEND=noninteractive
export TZ="Europe/Warsaw"
export PATH="/root/.local/bin:/root/bin:/root/usr/bin:$PATH"
export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig"

mkdir -p "${MCM_DIR}" "${MTL_DIR}" "${DPDK_DIR}" "${XDP_DIR}" "${GRPC_DIR}" "${JPEGXS_DIR}" "${LIBFABRIC_DIR}"

# Install package dependencies
apt-get update --fix-missing && \
apt-get full-upgrade -y && \
apt-get install --no-install-recommends -y \
    nasm cmake libbsd-dev git build-essential sudo \
    meson python3-dev python3-pyelftools pkg-config \
    libnuma-dev libjson-c-dev libpcap-dev libgtest-dev \
    libsdl2-dev libsdl2-ttf-dev libssl-dev ca-certificates \
    m4 clang llvm zlib1g-dev libelf-dev libcap-ng-dev \
    gcc-multilib systemtap-sdt-dev librdmacm-dev \
    libfdt-dev autoconf automake autotools-dev libtool && \

# Clone MTL, DPDK and xdp-tools repo
git clone --branch main https://github.com/OpenVisualCloud/Media-Transport-Library "${MTL_DIR}" && \
git -C "${MTL_DIR}" checkout ${MTL_VER} && \
git clone --branch v${DPDK_VER} --depth 1 https://github.com/DPDK/dpdk.git "${DPDK_DIR}" && \
git clone --recurse-submodules https://github.com/xdp-project/xdp-tools.git "${XDP_DIR}" && \
git clone --depth 1 --branch v1.22.0 https://github.com/ofiwg/libfabric "${LIBFABRIC_DIR}" && \
git clone --branch ${GPRC_VER} --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc "${GRPC_DIR}" && \
git clone https://github.com/OpenVisualCloud/SVT-JPEG-XS "${JPEGXS_DIR}" && \
git -C "${JPEGXS_DIR}" checkout ${JPEGXS_VER} && \

# Build the xdp-tools project
pushd "${XDP_DIR}"
./configure && \
make && \
make install && \
make -C "${XDP_DIR}/lib/libbpf/src" && \
make -C "${XDP_DIR}/lib/libbpf/src" install
popd

pushd "${LIBFABRIC_DIR}"
./autogen.sh && \
./configure && \
make -j$(nproc) && \
make install && \
popd

# Build the DPDK
pushd "${DPDK_DIR}"
git am "${MTL_DIR}/patches/dpdk/${DPDK_VER}/"*.patch
meson setup build && \
ninja -C build && \
meson install -C build
popd

# Build MTL
pushd "${MTL_DIR}"
./build.sh && \
meson install -C build
popd

# gRPC
mkdir -p "${GRPC_DIR}/cmake/build"
pushd "${GRPC_DIR}/cmake/build"
cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON ../.. && \
make -j $(nproc) && \
make install && \
cmake -DgRPC_BUILD_TESTS=ON -DgRPC_INSTALL=ON ../.. && \
make -j $(nproc) grpc_cli && \
cp grpc_cli "/usr/local/bin/"
popd

# Build and install JPEG XS
mkdir -p "${JPEGXS_DIR}/Build/linux" "${JPEGXS_DIR}/imtl-plugin"
pushd "${JPEGXS_DIR}/Build/linux"
./build.sh release install
popd

# Build and install JPEG XS imtl plugin
pushd "${JPEGXS_DIR}/imtl-plugin"
./build.sh build-only && \
meson install --no-rebuild -C build && \
mkdir -p /usr/local/etc/ && \
cp -f "${JPEGXS_DIR}/imtl-plugin/kahawai.json" /usr/local/etc/imtl.json
popd

export KAHAWAI_CFG_PATH="/usr/local/etc/imtl.json"
