#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
export WORKING_DIR="${BUILD_DIR:-${REPO_DIR}/build/rdma}"
export PERF_DIR="${DRIVERS_DIR}/perftest"

. "${SCRIPT_DIR}/common.sh"

function install_os_dependencies()
{
    log_info Starting: OS packages installation.
    log_info Starting: OS package manager auto-detection.
    setup_package_manager "apt-get"
    if [[ "${PM}" == "apt" || "${PM}" == "apt-get" ]]; then
        log_success "Found ${PM}. Using Ubuntu package dependencies approach"
        as_root "${PM}" update --fix-missing && \
        as_root "${PM}" install -y \
            cmake \
            cython3 \
            debhelper \
            dh-systemd \
            dh-python \
            dpkg-dev \
            libnl-3-dev \
            libnl-route-3-dev \
            libsystemd-dev \
            libudev-dev \
            ninja-build \
            pandoc \
            pkg-config \
            python3-dev \
            python3-docutils \
            valgrind \
            build-essential \
            gcc \
            python3-pyverbs \
            libelf-dev \
            infiniband-diags \
            rdma-core \
            ibverbs-utils \
            perftest \
            ethtool

    elif [[ "${PM}" == "yum" || "${PM}" == "dnf" ]]; then
        log_success "Found ${PM}. Using CentOS package dependencies approach"
        as_root "${PM}" -y update && \
        as_root "${PM}" -y install ethtool && \
        as_root "${PM}" builddep redhat/rdma-core.spec
    else
        log_error "Exiting: No supported package manager found. Contact support"
        exit 1
    fi
    log_info "Finished: Successful OS packages installation."
    log_warning OS reboot is required for all of the changes to take place.
    return 0
}

function get_irdma_driver_tgz() {
    echo "https://downloadmirror.intel.com/${IRDMA_DMID}/irdma-${IRDMA_VER}.tgz"
}

function get_and_patch_intel_drivers()
{
    log_info "Intel drivers: Starting download and patching actions."
    if [[ ! -d "${MTL_DIR}" ]]; then
        git_download_strip_unpack "OpenVisualCloud/Media-Transport-Library" "${MTL_VER}" "${MTL_DIR}"
    fi
    if [ ! -d "${MTL_DIR}/patches/ice_drv/${ICE_VER}/" ]; then
        log_error  "MTL patch for ICE=v${ICE_VER} could not be found: ${MTL_DIR}/patches/ice_drv/${ICE_VER}"
        return 1
    fi
    IRDMA_REPO="$(get_irdma_driver_tgz)"
    wget_download_strip_unpack "${IRDMA_REPO}" "${IRDMA_DIR}" && \
    git_download_strip_unpack "intel/ethernet-linux-ice"  "refs/tags/v${ICE_VER}"  "${ICE_DIR}"

    pushd "${ICE_DIR}" && \
    patch -p1 -i <(cat "${MTL_DIR}/patches/ice_drv/${ICE_VER}/"*.patch) && \
    popd && \
    { log_success "Intel drivers: Finished download and patching actions." && return 0; } ||
    { log_error "Intel drivers: Failed to download or patch." && return 1; }
}

function build_install_and_config_intel_drivers()
{
    log_info "Intel ICE: Driver starting the build and install workflow."
    if ! as_root make "-j${NPROC}" -C "${ICE_DIR}/src" install; then
        log_error "Intel ICE: Failed to build and install drivers"
        exit 5
    fi
    as_root rmmod irdma 2>&1 || true
    as_root rmmod ice
    as_root modprobe ice
    log_success "Intel ICE: Drivers finished install process."

    return 0
}

function build_install_and_config_irdma_drivers()
{
    if pushd "${IRDMA_DIR}"; then

        "${IRDMA_DIR}/build_core.sh" -y || exit 2
        as_root "${IRDMA_DIR}/install_core.sh"  || exit 3

        if as_root "${IRDMA_DIR}/build.sh"; then
            popd || log_warning "Intel irdma: Could not popd (directory). Ignoring."
            log_success "Intel irdma: Finished configuration and installation successfully."
            as_root rmmod irdma 2>&1 || true
            as_root modprobe irdma
            return 0
        fi

        log_error "Intel irdma: Errors while building '${IRDMA_DIR}/build.sh'"
        popd || log_warning "Intel irdma: Could not popd (directory). Ignoring."
    fi
    log_error "Intel irdma: Error while performing configuration/installation."
    return 1
}

function config_intel_rdma_driver()
{
    #   \s - single (s)pace or tabulator
    #   \d - single (d)igit
    # ^\s* - starts with zero or more space/tabulators
    # \s\+ - at least one or more space/tabulators
    local PREFIX_REGEX='^\s*options\s\+irdma\s\+'
    local PREFIX_NORM_ROCE='options\ irdma\ roce_ena=1'
    local PREFIX_NORM_SEL='options\ irdma\ limits_sel=5'

    log_info "Configuration of iRDMA starting."
    as_root mkdir -p "/etc/modprobe.d"
    as_root touch "/etc/modprobe.d/irdma.conf"

    log_info "Enabling RoCE."
    if grep -e "${PREFIX_REGEX}roce_ena=" /etc/modprobe.d/irdma.conf 1>/dev/null 2>&1; then
        as_root sed -i "s/${PREFIX_REGEX}roce_ena=\d/${PREFIX_NORM_ROCE}/g" /etc/modprobe.d/irdma.conf
    else
        echo "${PREFIX_NORM_ROCE}" | as_root tee -a /etc/modprobe.d/irdma.conf
    fi
    log_success "RoCE enabled."

    log_info "Increasing Queue Pair limit."
    if grep -e "${PREFIX_REGEX}limits_sel=" /etc/modprobe.d/irdma.conf 1>/dev/null 2>&1; then
        as_root sed -i "s/${PREFIX_REGEX}limits_sel=\d/${PREFIX_NORM_SEL}/g" /etc/modprobe.d/irdma.conf
    else
        echo "${PREFIX_NORM_SEL}" | as_root tee -a /etc/modprobe.d/irdma.conf
    fi
    as_root dracut -f
    log_success "Queue Pair limits_sel set to 5."
    log_success "Configuration of iRDMA finished."
}


# Allow sourcing of the script.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]
then
    set -exEo pipefail
    install_os_dependencies && \
    get_and_patch_intel_drivers && \
    build_install_and_config_intel_drivers
    build_install_and_config_irdma_drivers && \
    config_intel_rdma_driver
    return_code="$?"
    [[ "${return_code}" == "0" ]] && { log_success "Finished: Build, install and configuration of Intel drivers."; exit 0; }

    log_error "Intel drivers configuration/installation failed."
    exit "${return_code}"
fi
