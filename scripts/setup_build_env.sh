#!/bin/bash

set -eEo pipefail
set +x

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
VERSIONS_FILE_PATH="$(readlink -f "${VERSIONS_FILE_PATH:-${REPO_DIR}/versions.env}")"
BUILD_DIR="${BUILD_DIR:-${REPO_DIR}/_build}"
PREFIX_DIR="${PREFIX_DIR:-${REPO_DIR}/_install}"
DRIVERS_DIR="${DRIVERS_DIR:-/opt/intel/drivers}"

. "${VERSIONS_FILE_PATH}"
. "${SCRIPT_DIR}/common.sh"

ICE_DIR="${DRIVERS_DIR}/ice/${ICE_VER}"
IAVF_DIR="${DRIVERS_DIR}/iavf/${IAVF_VER}"
IRDMA_DIR="${DRIVERS_DIR}/irdma/${IRDMA_VER}"

export PM="${PM:-apt-get}"
export TZ="${TZ:-Europe/Warsaw}"
export NPROC="${NPROC:-$(nproc)}"
export DEBIAN_FRONTEND="noninteractive"

export KERNEL_VERSION="${KERNEL_VERSION:-$(uname -r)}"
export INSTALL_USE_CUSTOM_PATH="${INSTALL_USE_CUSTOM_PATH:-false}"

SYS_PKG_CONFIG_PATH="${SYS_PKG_CONFIG_PATH:-/usr/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig}"
if [[ "${PKG_CONFIG_PATH}" != *"${SYS_PKG_CONFIG_PATH}"* ]]; then
    export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${SYS_PKG_CONFIG_PATH}"
fi

mkdir -p "${PREFIX_DIR}/usr/lib/x86_64-linux-gnu" \
         "${PREFIX_DIR}/usr/local/lib/x86_64-linux-gnu" \
         "${PREFIX_DIR}/usr/local/include" \
         "${PREFIX_DIR}/usr/include"

MTL_DIR="${BUILD_DIR}/mtl"
DPDK_DIR="${BUILD_DIR}/dpdk"
XDP_DIR="${BUILD_DIR}/xdp"
BPF_DIR="${XDP_DIR}/lib/libbpf"
GRPC_DIR="${BUILD_DIR}/grpc"
JPEGXS_DIR="${BUILD_DIR}/jpegxs"
LIBFABRIC_DIR="${BUILD_DIR}/libfabric"
LIBFDT_DIR="${BUILD_DIR}/libfdt"
JSONC_DIR="${BUILD_DIR}/json-c"
NASM_DIR="${BUILD_DIR}/nasm"
MCM_SDK_DIR="${BUILD_DIR}/mcm-sdk"

function install_package_dependencies()
{
    setup_package_manager "apt-get"
    if [[ "${PM}" == "apt" || "${PM}" == "apt-get" ]]; then
        install_ubuntu_package_dependencies
    elif [[ "${PM}" == "yum" || "${PM}" == "dnf" ]]; then
        install_yum_package_dependencies
    else
        log_error  No supported package manager found
        exit 1
    fi
}
function install_ubuntu_package_dependencies()
{
    APT_LINUX_HEADERS="linux-headers-${KERNEL_VERSION}"
    APT_LINUX_MOD_EXTRA="linux-modules-extra-${KERNEL_VERSION}"
    as_root "${PM}" update --fix-missing && \
    as_root "${PM}" install --no-install-recommends -y \
        apt-transport-https \
        autoconf \
        automake \
        autotools-dev \
        build-essential \
        ca-certificates \
        clang \
        curl \
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
        "${APT_LINUX_HEADERS}" \
        "${APT_LINUX_MOD_EXTRA}" && \
    return 0 || return 1
}

function install_yum_package_dependencies()
{
    as_root "${PM}" -y update && \
    as_root "${PM}" -y install epel-release && \
    as_root "${PM}" -y makecache --refresh && \
    as_root "${PM}" -y distro-sync && \
    as_root "${PM}" -y group install "Development Tools" && \
    as_root "${PM}" -y install \
        autoconf \
        automake \
        bzip2 \
        curl-minimal \
        ca-certificates \
        clang \
        cmake \
        dracut \
        dtc \
        elfutils-libelf-devel \
        git \
        glibc-devel.i686 \
        glibc-devel.x86_64 \
        gtest-devel \
        intel-ipp-crypto-mb \
        intel-ipsec-mb \
        kernel-devel \
        kernel-headers \
        kernel-modules-extra \
        libbpf \
        libbsd-devel \
        libcap-ng-devel \
        libfdt \
        libibverbs-devel \
        libjson* \
        libpcap-devel \
        librdmacm-devel \
        libtool \
        llvm \
        libxdp \
        m4 \
        nss \
        numactl-devel \
        openssl-devel \
        pkg-config \
        python3 \
        python3-devel \
        python3-pyelftools \
        rdma-core \
        sudo \
        systemtap-sdt-devel \
        SDL2-devel \
        SDL2_net-devel \
        SDL2_ttf-devel \
        texinfo \
        wget \
        zlib-devel && \
    python3 -m pip install meson ninja && \
    lib_install_nasm_from_rpm && \
    lib_build_and_install_libfdt && \
    lib_build_and_install_jsonc && \
    return 0 || return 1
}
# Download and unpack dependencies from source code.
function get_download_unpack_dependencies()
{
    mkdir -p "${GRPC_DIR}"
    if ! git -C "${GRPC_DIR}" log -g --abbrev-commit --pretty=oneline HEAD "refs/tags/${GPRC_VER}" > /dev/null; then
        git clone --branch "${GPRC_VER}" --depth 1 --recurse-submodules --shallow-submodules https://github.com/grpc/grpc "${GRPC_DIR}"
    fi
    git_download_strip_unpack "xdp-project/xdp-tools" "${XDP_VER}" "${XDP_DIR}"
    git_download_strip_unpack "libbpf/libbpf" "${BPF_VER}" "${BPF_DIR}"
    git_download_strip_unpack "ofiwg/libfabric" "${LIBFABRIC_VER}" "${LIBFABRIC_DIR}"
    git_download_strip_unpack "OpenVisualCloud/Media-Transport-Library" "${MTL_VER}" "${MTL_DIR}"
    git_download_strip_unpack "dpdk/dpdk" "refs/tags/v${DPDK_VER}" "${DPDK_DIR}"
    git_download_strip_unpack "OpenVisualCloud/SVT-JPEG-XS" "${JPEGXS_VER}" "${JPEGXS_DIR}"
    patch -d "${DPDK_DIR}" -p1 -i <(cat "${MTL_DIR}/patches/dpdk/${DPDK_VER}/"*.patch)
}

function get_and_patch_intel_drivers()
{
    if [ ! -d "${MTL_DIR}/patches/ice_drv/${ICE_VER}/" ]; then
        log_error  "MTL patch for ICE=v${ICE_VER} could not be found: ${MTL_DIR}/patches/ice_drv/${ICE_VER}"
        return 1
    fi
    wget_download_strip_unpack "${IRDMA_REPO}" "${IRDMA_DIR}"
    git_download_strip_unpack "intel/ethernet-linux-iavf" "refs/tags/v${IAVF_VER}" "${IAVF_DIR}"
    git_download_strip_unpack "intel/ethernet-linux-ice"  "refs/tags/v${ICE_VER}"  "${ICE_DIR}"

    pushd "${ICE_DIR}"
    patch -p1 -i <(cat "${MTL_DIR}/patches/ice_drv/${ICE_VER}/"*.patch)
    popd
}

function build_install_and_config_intel_drivers()
{
    as_root make "-j${NPROC}" -C "${IAVF_DIR}/src" install && \
    as_root make "-j${NPROC}" -C "${ICE_DIR}/src" install && \
    return 0 || return 1
}

function build_install_and_config_irdma_drivers()
{
    pushd "${IRDMA_DIR}" && \
    as_root ./build.sh && \
    popd && \
    as_root config_intel_rdma_driver && \
    as_root modprobe irdma && \
    return 0 || return 1
}

# Download and install rpm repo for nasm
function lib_install_nasm_from_rpm()
{
    mkdir -p "${NASM_DIR}"
    curl -Lf "${NASM_RPM_LINK}" -o "${NASM_DIR}/nasm-2.rpm"
    as_root "${PM}" -y localinstall "${NASM_DIR}/nasm-"*.rpm
}

# Build and install libfdt from source code
function lib_build_and_install_libfdt()
{
    git_download_strip_unpack "dgibson/dtc" "refs/tags/${LIBFDT_VER}" "${LIBFDT_DIR}"
    make -j "${NPROC}" -C "${LIBFDT_DIR}"
    as_root make -j "${NPROC}" -C "${LIBFDT_DIR}" install
}

# Build and install json-c from source code
function lib_build_and_install_jsonc()
{
    git_download_strip_unpack "json-c/json-c/archive" "refs/tags/json-c-${JSON_C_VER}" "${JSONC_DIR}"
    mkdir -p "${JSONC_DIR}/json-c-build"
    cmake -S "${JSONC_DIR}" -B "${JSONC_DIR}/json-c-build" -DCMAKE_BUILD_TYPE=Release
    make -j "${NPROC}" -C "${JSONC_DIR}/json-c-build"
    as_root make -j "${NPROC}" -C "${JSONC_DIR}/json-c-build" install
}

# Build the xdp-tools project with ebpf
function lib_install_xdp_bpf_tools()
{
    pushd "${XDP_DIR}" && \
    ./configure && \
    make -j "${NPROC}" && \
    as_root make -j "${NPROC}" install && \
    DESTDIR="${PREFIX_DIR}" make -j "${NPROC}" install && \
    make -j "${NPROC}" -C "${XDP_DIR}/lib/libbpf/src" && \
    as_root make -j "${NPROC}" -C "${XDP_DIR}/lib/libbpf/src" install && \
    DESTDIR="${PREFIX_DIR}" make -j "${NPROC}" -C "${XDP_DIR}/lib/libbpf/src" install && \
    popd && \
    return 0 || return 1
}

# Build and install the libfabrics
function lib_install_fabrics()
{
    pushd "${LIBFABRIC_DIR}" && \
    ./autogen.sh && \
    ./configure --enable-verbs && \
    make -j "${NPROC}" && \
    as_root make -j "${NPROC}" install && \
    DESTDIR="${PREFIX_DIR}" make install && \
    popd && \
    return 0 || return 1
}

# Build and install the DPDK
function lib_install_dpdk()
{
    pushd "${DPDK_DIR}" && \
    meson setup build && \
    ninja -j "${NPROC}" -C build && \
    as_root meson install -C build && \
    DESTDIR="${PREFIX_DIR}" as_root meson install -C build && \
    popd && \
    return 0 || return 1
}

# Build and install the MTL
function lib_install_mtl()
{
    pushd "${MTL_DIR}" && \
    as_root ./build.sh release && \
    DESTDIR="${PREFIX_DIR}" as_root meson install -C build && \
    as_root install script/nicctl.sh "${PREFIX_DIR:-}/usr/local/bin" && \
    popd && \
    return 0 || return 1
}

# Build and install gRPC from source
function lib_install_grpc()
{
    mkdir -p "${GRPC_DIR}/cmake/build" && \
    pushd "${GRPC_DIR}/cmake/build" && \
    cmake -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON ../.. && \
    make -j "${NPROC}" && \
    as_root make -j "${NPROC}" install && \
    popd && \
    return 0 || return 1
}

# Build and install JPEG XS
function lib_install_jpeg_xs()
{
    mkdir -p "${JPEGXS_DIR}/Build/linux" && \
    pushd "${JPEGXS_DIR}/Build/linux" && \
    as_root ./build.sh release install && \
    as_root ./build.sh release --prefix "${PREFIX_DIR:-}/usr/local" install && \
    popd && \
    return 0 || return 1
}

# Build and install JPEG XS imtl plugin
function lib_install_mtl_jpeg_xs_plugin()
{
    mkdir -p "${JPEGXS_DIR}/imtl-plugin" "${PREFIX_DIR:-}/usr/local/etc/" && \
    pushd "${JPEGXS_DIR}/imtl-plugin" && \
    ./build.sh build-only && \
    as_root DESTDIR="${PREFIX_DIR}" meson install --no-rebuild -C build && \
    as_root install "${JPEGXS_DIR}/imtl-plugin/kahawai.json" "${PREFIX_DIR:-}/usr/local/etc/imtl.json" && \
    popd && \
    return 0 || return 1
}

# Build and install MCM SDK
function lib_build_install_sdk_mcm()
{
    pushd "${REPO_DIR}/sdk" && \
    ./build.sh && \
    popd && \
    return 0 || return 1
}

function full_build_and_install_workflow()
{
    lib_install_grpc && \
    lib_install_xdp_bpf_tools && \
    lib_install_fabrics && \
    lib_install_dpdk && \
    lib_install_mtl && \
    lib_install_jpeg_xs && \
    lib_install_mtl_jpeg_xs_plugin && \
    chmod -R a+r "${BUILD_DIR}" && \
    return 0 || return 1
}
# cp -f "${REPO_DIR}/media-proxy/imtl.json" "/usr/local/etc/imtl.json"
# export KAHAWAI_CFG_PATH="/usr/local/etc/imtl.json"

# Allow sourcing of the script.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]
then
    if [ "${EUID}" != "0" ]; then
        log_error "Must be run as root. Try running bellow command:"
        log_error "sudo \"${BASH_SOURCE[0]}\""
        exit 1
    fi
    print_logo_anim "2" "0.04"
    sleep 1
    trap_error_print_debug
    sleep 1
    set -x
    log_info Starting: OS packages installation, MTL and DPDK download.
    install_package_dependencies
    get_download_unpack_dependencies
    log_info Finished: OS packages installation, MTL and DPDK download.
    log_info Starting: Intel drivers download and patch apply.
    get_and_patch_intel_drivers
    log_info Finished: Intel drivers download and patch apply.
    log_info Starting: Dependencies build, install and configation.
    full_build_and_install_workflow || \
    { log_error  Dependencies build, install and configation failed. && exit 1; }
    log_info Finished: Dependencies build, install and configation.
    log_info Starting: Build, install and configuration of Intel drivers.
    build_install_and_config_intel_drivers || \
    { log_error  Intel drivers configuration/installation failed. && exit 1; }
    build_install_and_config_irdma_drivers || \
    { log_error  Intel irdma configuration/installation failed. && exit 1; }
    log_info Finished: Build, install and configuration of Intel drivers.
    log_info All tasks compleated. Reboot required.
    log_warning ""
    log_warning OS reboot is required for all of the changes to take place.
    sleep 2
    set +x
    exit 0
fi
