#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
export WORKING_DIR="${BUILD_DIR:-${REPO_DIR}/build/rdma}"
export PERF_DIR="${DRIVERS_DIR}/perftest"

. "${SCRIPT_DIR}/setup_build_env.sh"

function print_usage()
{
    log_info "Usage:"
    log_info "\t $0 [option]"
    log_info ""
    log_info "Options:"
    log_info ""
    log_info "\tall"
    log_info "\t\t get, build, and install whole stack."
    log_info "\tget-ice"
    log_info "\t\t get, build, and install ice, rdma, and irdma drivers stack."
    log_info "\tget-irdma"
    log_info "\t\t setup and pre-configure rdma, irdma and libfabrics ."
    log_info "\tget-perftest"
    log_info "\t\t download and install perftest and dependencies."
    log_info "\tcheck-mtu"
    log_info "\t\t fast-check basics in environment."
    log_info "\tset-mtu <INTERFACE>"
    log_info "\t\t temporarily set MTU to 9000 for given interface."
    log_info "\trun-perftest <INTERFACE>"
    log_info "\t\t execute installed perftests."
    log_info "" 
    log_info "\tintel"
    log_info "\t\t animation in bash"
}

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
    if [[ -d "${ICE_DIR}" ]]; then
        rm -rf "${ICE_DIR}"
    fi
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
    IRDMA_REPO="$(get_irdma_driver_tgz)"
    wget_download_strip_unpack "${IRDMA_REPO}" "${IRDMA_DIR}"
    
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
    log_success "Configuration of irdma finished."
}

function install_perftest()
{
    log_info "Start of install_perftest method. Installing apt packages."
    apt-get update --fix-missing && \
    apt install -y jq libibverbs-dev librdmacm-dev libibumad-dev libpci-dev
    log_info "Install_perftest method. Installing apt packages succeeded."

    log_info "Install_perftest method. Downloading and installing perftest-24.07.0."
    wget_download_strip_unpack "${PERF_REPO}" "${PERF_DIR}"

    if pushd "${PERF_DIR}"; then
       { ./autogen.sh && ./configure; } || \
       { log_error "Intel install_perftest: Could not configure the perftest."; return 1; }
    else
        log_error "Intel install_perftest: Could not enter the directory ${PERF_DIR}, exiting."
        exit 1
    fi
    make -j "${NPROC}" && \
    as_root make install || \
    { log_error log_error "Intel irdma: Could not make and/or install the perftest."; return 1; }

    popd || log_warning "Intel irdma: Could not popd (directory). Ignoring."
    log_info "End of install_perftest method. Finished installing perftest-24.07.0."
}

function check_roce()
{
    log_info -n "Checking RoCE... "
    roce_ena_val=$(cat /sys/module/irdma/parameters/roce_ena)
    if [[ "$roce_ena_val" != "1" ]] && [[ "$roce_ena_val" != "65535" ]]; then
        log_error  "FAIL: RoCE disabled"
        exit 1
    fi
    log_info "OK"
}

function check_mtu()
{
    log_info "Checking MTU..."
    rdma_devices=()
    rdma_links="$(rdma link -j | jq -r '.[] | select(.physical_state == "LINK_UP") | .netdev')"
    mapfile -t rdma_devices <<< "${rdma_links}"
    if [[ "${#rdma_devices[@]}" == "0" ]]; then
        log_error  "FAIL: No RDMA devices detected"
        exit 1
    fi
    fail=0
    for device in "${rdma_devices[@]}"; do
        mtu=$(ip addr | grep "$device:" | cut -d " " -f 5)
        if [[ "$mtu" == "9000" ]]; then
            log_info "* $device OK"
        else
            log_error  "* $device FAIL: MTU = $mtu (9000 is recommended)"
            fail=1
        fi
    done
    if [[ $fail -eq 1 ]]; then
        exit 1
    fi
}

function set_mtu()
{
    log_info -n "Setting MTU to 9000 on $1... "
    ip link set dev "${1}" mtu 9000
    log_info "DONE"
}

function run_perftest()
{
    # Usage: run_perftest <interface_name> [network_address] [network_mask]
    interface_name="$1"
    network_address="${2:-192.168.255.255}"
    network_mask="${3:-24}"
    ip addr show dev "${interface_name}" | grep "${network_address}" || \
    ip addr add "${network_address}/${network_mask}" dev "${interface_name}"
    taskset -c 1 ib_write_bw --qp=4 --report_gbit -D 60 --tos 96 -R &> "${WORKING_DIR}/perftest_server.log" &
    server_pid=$!
    ib_write_bw "${network_address}" --qp=4 --report_gbit -D 60 --tos 96 -R &> "${WORKING_DIR}/perftest_client.log" &
    client_pid=$!
    log_info "perftest is running. Waiting 60 s..."
    wait $server_pid
    wait $client_pid
    log_info "perftest completed. See results in $WORKING_DIR/perftest_server.log and ${WORKING_DIR}/perftest_client.log"
    ip addr del "${network_address}/${network_mask}" dev "${interface_name}"
}

# Allow sourcing of the script.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]
then
  if [ "${EUID}" != "0" ]; then
    log_error "Must be run as root. Try running below command:"
    log_error "sudo \"${BASH_SOURCE[0]}\""
    exit 1
  fi

  if [[ -f "${WORKING_DIR}" ]]; then
    log_error "Can't create rdma directory because of the rdma file ${WORKING_DIR}"
    exit 1
  fi
  rm -f "./${WORKING_DIR}/*"
  mkdir -p "${WORKING_DIR}" "${PERF_DIR}"

  set -eEo pipefail  
 
  if [[ "${1}" == "get-ice" || "${1}" == "all" ]]; then
    install_os_dependencies && \
    get_and_patch_intel_drivers && \
    build_install_and_config_intel_drivers
    return_code="$?"
    if [[ "${return_code}" == "0" ]]; then
      log_success "Finished: Build and install and configuration of Intel drivers.";
    else
      log_error "Intel drivers configuration/installation failed."
    fi
  fi
  if [[ "${1}" == "get-irdma" || "${1}" == "all" ]]; then
    if [[ "${1}" == "get-irdma" ]]; then
      install_os_dependencies && \
      lib_install_fabrics
    fi
    build_install_and_config_irdma_drivers && \
    config_intel_rdma_driver
    return_code="$?"
    if [[ "${return_code}" == "0" ]]; then
      log_success "Finished: irdma driver configuration for Intel hardware backend."
    else
      log_error "Intel irdma configuration failed."
      exit "${return_code}"
    fi
  fi
  if [[ "$1" == "get-perftest" || "${1}" == "all" ]]; then
    install_perftest
  fi
  if [[ "$1" == "check-mtu" ]]; then
    check_roce
    check_mtu
  elif [[ "$1" == "set-mtu" ]]; then
    MCM_MTU_OVERRIDE="${2:-$MCM_MTU_OVERRIDE}"
    set_mtu "${MCM_MTU_OVERRIDE:-9000}"
    check_mtu
  elif [[ "$1" == "run-perftest" ]]; then
    run_perftest "$@"
  elif [[ "$1" == "intel" ]]; then
    print_logo_anim
  else
    print_usage
  fi
fi
