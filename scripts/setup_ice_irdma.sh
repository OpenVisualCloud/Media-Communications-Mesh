#!/usr/bin/env bash

set -eEo pipefail
set +x

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
export WORKING_DIR="${BUILD_DIR:-${REPO_DIR}/build/rdma}"
export PERF_DIR="${DRIVERS_DIR}/perftest"

. "${SCRIPT_DIR}/common.sh"

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
    wget_download_strip_unpack "${IRDMA_REPO}" "${IRDMA_DIR}" && \
    git_download_strip_unpack "intel/ethernet-linux-iavf" "refs/tags/v${IAVF_VER}" "${IAVF_DIR}" && \
    git_download_strip_unpack "intel/ethernet-linux-ice"  "refs/tags/v${ICE_VER}"  "${ICE_DIR}"

    pushd "${ICE_DIR}" && \
    patch -p1 -i <(cat "${MTL_DIR}/patches/ice_drv/${ICE_VER}/"*.patch) && \
    popd && \
    { log_success "Intel drivers: Finished download and patching actions." && return 0; } ||
    { log_error "Intel drivers: Failed to download or patch." && return 1; }
}

function build_install_and_config_intel_drivers()
{
    # TO-DO: Check why installing IAVF break the ICE and IRDMA workflow.

    # log_info "Intel IAVF: Driver starting the build and install workflow."
    # if as_root make "-j${NPROC}" -C "${IAVF_DIR}/src" install; then
    # fi

    log_info "Intel ICE: Driver starting the build and install workflow."
    if as_root make "-j${NPROC}" -C "${ICE_DIR}/src" install; then
        log_success "Intel ICE: Drivers finished install process."
        return 0
    fi
    log_error "Intel ICE: Failed to build and install drivers"
    return 1
}

function build_install_and_config_irdma_drivers()
{
    if pushd "${IRDMA_DIR}" && as_root ./build.sh && as_root modprobe irdma && popd; then
        log_success "Intel irdma: Finished configuration and installation successfully."
        return 0
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
    get_and_patch_intel_drivers && \
    build_install_and_config_irdma_drivers && \
    build_install_and_config_intel_drivers && \
    config_intel_rdma_driver
    return_code="$?"
    [[ "${return_code}" == "0" ]] && { log_success "Finished: Build, install and configuration of Intel drivers."; exit 0; }

    log_error "Intel drivers configuration/installation failed."
    exit "${return_code}"
fi
