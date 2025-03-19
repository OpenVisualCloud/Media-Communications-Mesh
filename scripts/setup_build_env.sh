#!/bin/bash

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
. "${SCRIPT_DIR}/common.sh"

export PM="${PM:-apt-get}"
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

function install_package_dependencies()
{
    log_info Starting: OS packages installation.
    log_info Starting: OS package manager auto-detection.
    setup_package_manager "apt-get"
    if [[ "${PM}" == "apt" || "${PM}" == "apt-get" ]]; then
        log_success "Found ${PM}. Using Ubuntu package dependencies approach"
        install_ubuntu_package_dependencies
    elif [[ "${PM}" == "yum" || "${PM}" == "dnf" ]]; then
        log_success "Found ${PM}. Using CentOS package dependencies approach"
        install_yum_package_dependencies
    else
        log_error "Exiting: No supported package manager found. Contact support"
        exit 1
    fi
    log_info "Finished: Successful OS packages installation."
    log_warning OS reboot is required for all of the changes to take place.
    return 0
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
    lib_build_and_install_golang && \
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
    lib_build_and_install_golang && \
    return 0 || return 1
}

function apply_dpdk_patches()
{
    mapfile -t PATCH_LIST < <(find "${MTL_DIR}/patches/dpdk/${DPDK_VER}" -path "${MTL_DIR}/patches/dpdk/${DPDK_VER}/windows" -prune -o -name '*.patch' -print | sort)
    for pt in "${PATCH_LIST[@]}"; do
        log_info "Apply patch ${pt}"
        patch -d "${DPDK_DIR}" -p1 -i <(cat "${pt}")
        error="$?"
        if [[ "${error}" == "0" ]]; then
            log_success "Ok. ${pt}"
        else
            log_error "Error (${error}): Faild to apply ${pt}"
            exit 1
        fi
    done
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
    apply_dpdk_patches
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

# Get and install golang from source
function lib_build_and_install_golang()
{
    wget_download_strip_unpack "https://go.dev/dl/go${GOLANG_GO_VER}.linux-amd64.tar.gz" "${BUILD_DIR}/golang" && \
    as_root cp -r "${BUILD_DIR}/golang" "/usr/local/go" || true
    as_root ln -s /usr/local/go/bin/go /usr/bin/go || true
    go version && \
    go install "${GOLANG_PROTOBUF_GEN}" && \
    go install "${GOLANG_GRPC_GEN}" && \
    return 0 || return 1
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
    log_info Starting: Dependencies build, install and configation.
    lib_install_grpc && \
    lib_install_xdp_bpf_tools && \
    lib_install_fabrics && \
    lib_install_dpdk && \
    lib_install_mtl && \
    lib_install_jpeg_xs && \
    lib_install_mtl_jpeg_xs_plugin
    return_code="$?"
    if [[ "${return_code}" != "0" ]]; then
        log_error "Dependencies build, install and configation failed."
    else
        log_success "Finished: Dependencies build, install and configation."
    fi
    return "${return_code}"
}
# cp -f "${REPO_DIR}/media-proxy/imtl.json" "/usr/local/etc/imtl.json"
# export KAHAWAI_CFG_PATH="/usr/local/etc/imtl.json"

# Allow sourcing of the script.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]
then
    set -exEo pipefail
    if [ "${EUID}" != "0" ]; then
        log_error "Must be run as root. Try running below command:"
        log_error "sudo \"${BASH_SOURCE[0]}\""
        exit 3
    fi
    if [ "${NO_LOGO_PRINT:-0}" != "0" ]; then
        print_logo_anim "2" "0.04"
        sleep 2
    fi
    trap_error_print_debug
    install_package_dependencies
    get_download_unpack_dependencies
    full_build_and_install_workflow
    log_success "All tasks compleated successfuly. Happy MCM'ing ;-)"
    exit 0
fi
