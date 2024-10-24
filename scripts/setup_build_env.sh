#!/bin/bash

set -eo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"

. "${SCRIPT_DIR}/common.sh"

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

function get_and_patch_intel_drivers()
{
    set -x
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
    set +x
}

function build_install_and_config_intel_drivers()
{
    set -x
    make -j "${NPROC}" -C "${IAVF_DIR}/src" install
    make -j "${NPROC}" -C "${ICE_DIR}/src" install
    pushd "${IRDMA_DIR}"
    ./build.sh
    popd
    config_intel_rdma_driver
    modprobe irdma
    set +x
}

function install_ubuntu_package_dependencies()
{
    set -x
    APT_LINUX_HEADERS="linux-headers-$(uname -r)"
    apt-get update --fix-missing && \
    apt-get install --no-install-recommends -y \
        apt-transport-https \
        autoconf \
        automake \
        autotools-dev \
        build-essential \
        ca-certificates \
        clang \
        dracut \
        gcc-multilib \
        libbsd-dev \
        libcap-ng-dev \
        libelf-dev \
        libfdt-dev \
        libgtest-dev \
        rdma-core \
        libibverbs-dev \
        libjson-c-dev \
        libnuma-dev \
        libpcap-dev \
        librdmacm-dev \
        libsdl2-dev \
        libsdl2-ttf-dev \
        libssl-dev \
        libtool \
        llvm \
        m4 \
        meson \
        nasm cmake \
        pkg-config \
        python3-dev \
        python3-pyelftools \
        software-properties-common \
        sudo git \
        systemtap-sdt-dev \
        wget \
        zlib1g-dev \
        "${APT_LINUX_HEADERS}"
    set +x
    return 0
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
    ./configure --enable-verbs && \
    make -j "${NPROC}" && \
    make install
    ldconfig
    popd
}

# Download the MTL and DPDK source code
function lib_download_mtl_and_dpdk()
{
    git_download_strip_unpack "${MTL_REPO}" "${MTL_VER}" "${MTL_DIR}"
    git_download_strip_unpack "dpdk/dpdk" "refs/tags/v${DPDK_VER}" "${DPDK_DIR}"
}

# Build and install the MTL and DPDK
function lib_install_mtl_and_dpdk()
{
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
    ldconfig
    popd
}

# Build and install gRPC from source
function lib_install_grpc()
{
    rm -rf "${GRPC_DIR}"
    mkdir -p "${GRPC_DIR}"
    git clone --branch "${GPRC_VER}" \
        --depth 1 \
        --recurse-submodules \
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
    ldconfig
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
    set -x
    lib_install_xdp_bpf_tools
    lib_install_fabrics
    lib_install_mtl_and_dpdk
    lib_install_grpc
    lib_install_jpeg_xs
    lib_install_mtl_jpeg_xs_plugin
    chmod -R a+r "${BUILD_DIR}"
    set +x
}

if [ "${EUID}" != "0" ]; then
    error "Must be run as root. Try running bellow command:"
    error "sudo \"${BASH_SOURCE[0]}\""
    exit 1
fi

# cp -f "${REPO_DIR}/media-proxy/imtl.json" "/usr/local/etc/imtl.json"
# export KAHAWAI_CFG_PATH="/usr/local/etc/imtl.json"

print_logo_anim "2" "0.04"
sleep 1
trap_error_print_debug
sleep 1
prompt Starting: OS packages installation, MTL and DPDK download.
install_ubuntu_package_dependencies
lib_download_mtl_and_dpdk
prompt Finished: OS packages installation, MTL and DPDK download.
prompt Starting: Intel drivers download and patch apply.
get_and_patch_intel_drivers
prompt Finished: Intel drivers download and patch apply.
prompt Starting: Build, install and configuration of Intel drivers.
build_install_and_config_intel_drivers
prompt Finished: Build, install and configuration of Intel drivers.
prompt Starting: Dependencies build, install and configation.
full_build_and_install_workflow
prompt Finished: Dependencies build, install and configation.
prompt All tasks compleated successfully. Reboot required.
warning ""
warning OS reboot is required for all of the changes to take place.
sleep 2
exit 0
