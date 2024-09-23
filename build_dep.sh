#!/bin/bash

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="${SCRIPT_DIR:-$(pwd)}"

. "${REPO_DIR}/common.sh"

export MCM_DIR="${REPO_DIR}/build/mcm"
export MTL_VER="d2515b90cc0ef651f6d0a6661d5a644490bfc3f3"
export MTL_DIR="${REPO_DIR}/build/mtl"
export DPDK_VER="23.11"
export DPDK_DIR="${REPO_DIR}/build/dpdk"
export XDP_VER="d7edea3590052581c5fda5f8cfa40ae7be94f05c"
export XDP_DIR="${REPO_DIR}/build/xdp"
export BPF_VER="42065ea6627ff6e1ab4c65e51042a70fbf30ff7c"
export BPF_DIR="${XDP_DIR}/lib/libbpf"
export GPRC_VER="v1.58.0"
export GRPC_DIR="${REPO_DIR}/build/grpc"
export JPEGXS_VER="e1030c6c8ee2fb05b76c3fa14cccf8346db7a1fa"
export JPEGXS_DIR="${REPO_DIR}/build/jpegxs"
export LIBFABRIC_VER="v1.22.0"
export LIBFABRIC_DIR="${REPO_DIR}/build/libfabric"

export ICE_VERSION="1.14.9"
export ICE_DIR="/opt/intel/ice/${ICE_VERSION}"

export DEBIAN_FRONTEND=noninteractive
export TZ="Europe/Warsaw"
export PATH="/root/.local/bin:/root/bin:/root/usr/bin:$PATH"
export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig"

function wget_patch_build_install_intel_ice_driver()
{
    ICE_DOWNLOAD_URL="https://unlimited.dl.sourceforge.net/project/e1000/ice%20stable/${ICE_VERSION}/ice-${ICE_VERSION}.tar.gz"

    mkdir -p "${ICE_DIR}"
    wget --directory-prefix "${ICE_DIR}" "${ICE_DOWNLOAD_URL}"
    tar xvzf "${ICE_DIR}/ice-${ICE_VERSION}.tar.gz" --strip-components=1 -C "${ICE_DIR}"
    rm "${ICE_DIR}/ice-${ICE_VERSION}.tar.gz"
    for pt in "${MTL_DIR}/patches/ice_drv/${ICE_VERSION}/"*.patch; do patch -d "${ICE_DIR}" -p1 -i $pt; done

    make -C "${ICE_DIR}/src"
    make -C "${ICE_DIR}/src" install
}

# GITHUB_CREDENTIALS="username:password"
# URL construction: https://${GITHUB_CREDENTIALS}@github.com/${name}/archive/${version}.tar.gz
# $1 - name
# $2 - version
# $3 - dest_dir
function git_download_strip_unpack()
{
    # Version can be commit sha or tag, examples:
    # version=d2515b90cc0ef651f6d0a6661d5a644490bfc3f3
    # version=refs/tags/v${JPEG_XS_VER}
    name="${1}"
    version="${2}"
    dest_dir="${3}"
    filename="$(get_filename ${version})"
    [ -n "${GITHUB_CREDENTIALS}" ] && creds="${GITHUB_CREDENTIALS}@" || creds=""

    mkdir -p "${dest_dir}"
    curl -Lf https://${creds}github.com/${name}/archive/${version}.tar.gz -o "${dest_dir}/${filename}.tar.gz"
    tar -zx --strip-components=1 -C "${dest_dir}" -f "${dest_dir}/${filename}.tar.gz"
    rm -f "${dest_dir}/${filename}.tar.gz"
}

mkdir -p "${MCM_DIR}" "${MTL_DIR}" "${DPDK_DIR}" "${XDP_DIR}" "${GRPC_DIR}" "${JPEGXS_DIR}" "${LIBFABRIC_DIR}"

# Install package dependencies
apt-get update --fix-missing && \
apt-get full-upgrade -y && \
apt-get install --no-install-recommends -y \
    wget \
    nasm cmake \
    libbsd-dev \
    build-essential \
    sudo git \
    meson \
    python3-dev python3-pyelftools \
    pkg-config \
    libnuma-dev libjson-c-dev \
    libpcap-dev libgtest-dev \
    libsdl2-dev libsdl2-ttf-dev \
    libssl-dev ca-certificates \
    m4 clang llvm zlib1g-dev \
    libelf-dev libcap-ng-dev \
    gcc-multilib \
    systemtap-sdt-dev \
    librdmacm-dev \
    libfdt-dev \
    autoconf \
    automake \
    autotools-dev \
    libtool \
    grpc-proto \
    protobuf-compiler-grpc \
    libgrpc10

# Clone MTL, DPDK and xdp-tools repo
git_download_strip_unpack "OpenVisualCloud/Media-Transport-Library" "${MTL_VER}" "${MTL_DIR}"
git_download_strip_unpack "dpdk/dpdk" "refs/tags/v${DPDK_VER}" "${DPDK_DIR}"
git_download_strip_unpack "xdp-project/xdp-tools" "${XDP_VER}" "${XDP_DIR}"
git_download_strip_unpack "libbpf/libbpf" "${BPF_VER}" "${BPF_DIR}"
git_download_strip_unpack "ofiwg/libfabric" "${LIBFABRIC_VER}" "${LIBFABRIC_DIR}"
# git clone --branch ${GPRC_VER} --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc "${GRPC_DIR}" && \
git_download_strip_unpack "OpenVisualCloud/SVT-JPEG-XS" "${JPEGXS_VER}" "${JPEGXS_DIR}"

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
for pt in "${MTL_DIR}/patches/dpdk/${DPDK_VER}/"*.patch; do patch -p1 -i $pt; done
meson setup build && \
ninja -C build && \
meson install -C build
popd

# Build MTL
pushd "${MTL_DIR}"
./build.sh && \
meson install -C build
install script/nicctl.sh /usr/bin
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
"${REPO_DIR}/ffmpeg-plugin/clone-and-patch-ffmpeg.sh"
