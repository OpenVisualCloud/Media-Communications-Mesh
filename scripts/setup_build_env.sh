#!/bin/bash

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
BUILD_DIR="${BUILD_DIR:-${REPO_DIR}/build}"
DRIVERS_DIR="${DRIVERS_DIR:-/opt/intel/drivers}"

. "${SCRIPT_DIR}/common.sh"

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
export GPRC_VER="${GPRC_VER:-v1.58.0}"
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
    filename="$(get_filename "${version}")"
    [ -n "${GITHUB_CREDENTIALS}" ] && creds="${GITHUB_CREDENTIALS}@" || creds=""

    mkdir -p "${dest_dir}"
    curl -Lf "https://${creds}github.com/${name}/archive/${version}.tar.gz" -o "${dest_dir}/${filename}.tar.gz"
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

function build_install_and_config_intel_drivers()
{
    pushd "${IRDMA_DIR}"
    ./build.sh
    popd
    make -j "${NPROC}" -C "${IAVF_DIR}/src" install
    make -j "${NPROC}" -C "${ICE_DIR}/src" install
    config_intel_rdma_driver
}

function install_ubuntu_package_dependencies()
{
    # Install package dependencies
    apt-get update --fix-missing && \
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

# Build the xdp-tools project with ebpf
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

# Build and install gRPC from source
function lib_install_grpc()
{
    mkdir -p "${GRPC_DIR}"
    git clone --branch "${GPRC_VER}" \
        --recurse-submodules --depth 1 \
        --shallow-submodules \
        https://github.com/grpc/grpc "${GRPC_DIR}"

    mkdir -p "${GRPC_DIR}/cmake/build"
    pushd "${GRPC_DIR}/cmake/build"

    cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON ../.. && \
    make -j "${NPROC}" && \
    make -j "${NPROC}" install && \
    cmake -DgRPC_BUILD_TESTS=ON -DgRPC_INSTALL=ON ../.. && \
    make -j "${NPROC}" grpc_cli && \

    cp grpc_cli "/usr/local/bin/"
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
}

# Build and install JPEG XS imtl plugin
function lib_install_mtl_jpeg_xs_plugin()
{
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
    lib_install_mtl_jpeg_xs_plugin
}

if [ "${UID}" != "0" ]; then
    error This script must be run only as a root user.
    exit 1
fi

# cp -f "${REPO_DIR}/media-proxy/imtl.json" "/usr/local/etc/imtl.json"
# export KAHAWAI_CFG_PATH="/usr/local/etc/imtl.json"

print_logo_anim
sleep 1
prompt Starting: Dependencies build, install and configation.
full_build_and_install_workflow
prompt Finished: Dependencies build, install and configation.
prompt Starting: Intel drivers download and patch apply.
get_and_patch_intel_drivers
prompt Finished: Intel drivers download and patch apply.
prompt Starting: Build, install and configuration of Intel drivers.
build_install_and_config_intel_drivers
prompt Finished: Build, install and configuration of Intel drivers.
prompt All tasks compleated successfully. Reboot required.
warning ""
warning OS reboot is required for all of the changes to take place.
sleep 2
exit 0
