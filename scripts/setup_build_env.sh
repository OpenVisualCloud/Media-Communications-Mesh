#!/bin/bash

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
BUILD_DIR="${BUILD_DIR:-${REPO_DIR}/build}"
DRIVERS_DIR="${DRIVERS_DIR:-/opt/intel/drivers}"

. "${REPO_DIR}/common.sh"

NPROC="$(nproc)"
export MCM_DIR="${BUILD_DIR}/mcm"
export MTL_VER="${MTL_VER:-maint-24.09}" # maint-24.09
export MTL_DIR="${BUILD_DIR}/mtl"
export MTL_REPO="${MTL_REPO:-OpenVisualCloud/Media-Transport-Library}"
export DPDK_VER="${DPDK_VER:-23.11}"
export DPDK_DIR="${BUILD_DIR}/dpdk"
export XDP_VER="${XDP_VER:-d7edea3590052581c5fda5f8cfa40ae7be94f05c}"
export XDP_DIR="${BUILD_DIR}/xdp"
export BPF_VER="${BPF_VER:-42065ea6627ff6e1ab4c65e51042a70fbf30ff7c}"
export BPF_DIR="${XDP_DIR}/lib/libbpf"
export GPRC_VER="${GPRC_VER:-v1.58.3}"
export GRPC_DIR="${BUILD_DIR}/grpc"
export JPEGXS_VER="${JPEGXS_VER:-e1030c6c8ee2fb05b76c3fa14cccf8346db7a1fa}"
export JPEGXS_DIR="${BUILD_DIR}/jpegxs"
export LIBFABRIC_VER="${LIBFABRIC_VER:-v1.22.0}"
export LIBFABRIC_DIR="${BUILD_DIR}/libfabric"

export ICE_VER="${ICE_VER:-1.14.9}"
export ICE_DIR="${DRIVERS_DIR}/ice/${ICE_VER}"
export IAVF_VER="${IAVF_VER:-4.12.5}"
export IAVF_DIR="${DRIVERS_DIR}/iavf/${IAVF_VER}"
export IRDMA_DMID="${IRDMA_DMID:-832291}"
export IRDMA_VER="${IRDMA_VER:-1.15.11}"
export IRDMA_DIR="${DRIVERS_DIR}/irdma/${IRDMA_VER}"

export TZ="Europe/Warsaw"
export DEBIAN_FRONTEND="noninteractive"
export PATH="/root/.local/bin:/root/bin:/root/usr/bin:$PATH"
export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig"

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

function wget_download_strip_unpack()
{
    local filename
    local source_url="${1}"
    local dest_dir="${2}"
    filename="$(get_filename "${source_url}")"
    [ -n "${GITHUB_CREDENTIALS}" ] && creds="${GITHUB_CREDENTIALS}@" || creds=""

    mkdir -p "${dest_dir}"
    curl -Lf "${source_url}" -o "${dest_dir}/${filename}.tar.gz"
    tar -zx --strip-components=1 -C "${dest_dir}" -f "${dest_dir}/${filename}.tar.gz"
    rm -f "${dest_dir}/${filename}.tar.gz"
}

function get_and_patch_intel_drivers()
{
    if [ ! -d "${MTL_DIR}/patches/ice_drv/${ICE_VER}/" ]; then
        error "MTL patch for ICE=v${ICE_VER} could not be found: ${MTL_DIR}/patches/ice_drv/${ICE_VER}"
        return 1
    fi
    wget_download_strip_unpack "https://downloadmirror.intel.com/${IRDMA_DMID}/irdma-${IRDMA_VER}.tgz" "${IRDMA_DIR}"
    git_download_strip_unpack "intel/ethernet-linux-iavf" "refs/tags/v${IAVF_VER}" "${IAVF_DIR}"
    git_download_strip_unpack "intel/ethernet-linux-ice"  "refs/tags/v${ICE_VER}"  "${ICE_DIR}"

    pushd "${ICE_DIR}"
    patch -p1 -i <(cat "${MTL_DIR}/patches/ice_drv/${ICE_VER}/"*.patch)
    popd
}

function build_and_install_intel_drivers()
{
    pushd "${IRDMA_DIR}"
    ./build.sh
    popd
    make -C "${IAVF_DIR}/src" install
    make -C "${ICE_DIR}/src" install
}

function install_ubuntu_package_dependencies()
{
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
}

# git clone --branch ${GPRC_VER} --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc "${GRPC_DIR}" && \

# Build the xdp-tools project with bpf
function lib_install_xdp_bpf_tools()
{
    git_download_strip_unpack "xdp-project/xdp-tools" "${XDP_VER}" "${XDP_DIR}"
    git_download_strip_unpack "libbpf/libbpf" "${BPF_VER}" "${BPF_DIR}"

    pushd "${XDP_DIR}"
    ./configure && \
    make && \
    make install && \
    make -C "${XDP_DIR}/lib/libbpf/src" && \
    make -C "${XDP_DIR}/lib/libbpf/src" install
    popd
}

# Build and install the libfabrics
function lib_install_fabrics()
{
    git_download_strip_unpack "ofiwg/libfabric" "${LIBFABRIC_VER}" "${LIBFABRIC_DIR}"

    pushd "${LIBFABRIC_DIR}"
    ./autogen.sh && \
    ./configure && \
    make -j "${NPROC}" && \
    make install
    popd
}

# Build and install the MTL and DPDK
function lib_install_mtl_and_dpdk()
{
    git_download_strip_unpack "${MTL_REPO}" "${MTL_VER}" "${MTL_DIR}"
    git_download_strip_unpack "dpdk/dpdk" "refs/tags/v${DPDK_VER}" "${DPDK_DIR}"

    # Patch and build the DPDK
    pushd "${DPDK_DIR}"
    patch -p1 -i <(cat "${MTL_DIR}/patches/dpdk/${DPDK_VER}/"*.patch)
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
}

# Build and install gRPC
function lib_install_grpc()
{
    # git_download_strip_unpack "grpc/grpc" "${GPRC_VER}" "${GRPC_DIR}"
    # git_download_strip_unpack "abseil/abseil-cpp" "29bf8085f3bf17b84d30e34b3d7ff8248fda404e" "${GRPC_DIR}/third_party/abseil-cpp"
    # git_download_strip_unpack "c-ares/c-ares" "main" "${GRPC_DIR}/third_party/cares/cares"
    # git_download_strip_unpack "protocolbuffers/protobuf" "2c5fa078d8e86e5f4bd34e6f4c9ea9e8d7d4d44a" "${GRPC_DIR}/third_party/protobuf"
    # git_download_strip_unpack "google/re2" "0c5616df9c0aaa44c9440d87422012423d91c7d1" "${GRPC_DIR}/third_party/re2"
    # git_download_strip_unpack "madler/zlib" "04f42ceca40f73e2978b50e93806c2a18c1281fc" "${GRPC_DIR}/third_party/zlib"
    # git_download_strip_unpack "envoyproxy/data-plane-api" "main" "${GRPC_DIR}/third_party/envoy-api"
    # git_download_strip_unpack "googleapis/googleapis" "2f9af297c84c55c8b871ba4495e01ade42476c92" "${GRPC_DIR}/third_party/googleapis"
    # git_download_strip_unpack "google/googletest" "0e402173c97aea7a00749e825b194bfede4f2e45" "${GRPC_DIR}/third_party/googletest"
    # git_download_strip_unpack "census-instrumentation/opencensus-proto" "4aa53e15cbf1a47bc9087e6cfdca214c1eea4e89" "${GRPC_DIR}/third_party/opencensus-proto"
    # git_download_strip_unpack "open-telemetry/opentelemetry-proto" "main" "${GRPC_DIR}/third_party/opentelemetry"
    # git_download_strip_unpack "envoyproxy/protoc-gen-validate" "main" "${GRPC_DIR}/third_party/protoc-gen-validate"
    # git_download_strip_unpack "google/bloaty" "main" "${GRPC_DIR}/third_party/bloaty"
    # git_download_strip_unpack "google/boringssl" "main" "${GRPC_DIR}/third_party/boringssl-with-bazel"
    # git_download_strip_unpack "cncf/xds" "e9ce68804cb4e64cab5a52e3c8baf840d4ff87b7" "${GRPC_DIR}/third_party/xds"
    git clone --branch "${GPRC_VER}" --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc "${GRPC_DIR}" && \
    mkdir -p "${GRPC_DIR}/cmake/build"
    pushd "${GRPC_DIR}/cmake/build"
    cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON ../.. && \
    make -j "${NPROC}" && \
    make install
    popd
}

# Build and install JPEG XS
function lib_install_jpeg_xs()
{
    git_download_strip_unpack "OpenVisualCloud/SVT-JPEG-XS" "${JPEGXS_VER}" "${JPEGXS_DIR}"

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
}

function full_build_and_install_workflow()
{
    install_ubuntu_package_dependencies
    lib_install_xdp_bpf_tools
    lib_install_fabrics
    lib_install_mtl_and_dpdk
    lib_install_grpc
    lib_install_jpeg_xs
    "${REPO_DIR}/ffmpeg-plugin/clone-and-patch-ffmpeg.sh"
    cp -f "${REPO_DIR}/media-proxy/imtl.json" "/usr/local/etc/imtl.json"
    export KAHAWAI_CFG_PATH="/usr/local/etc/imtl.json"
# get_and_patch_intel_drivers
# build_and_install_intel_drivers
}

full_build_and_install_workflow
