#!/usr/bin/env bash

set -eEo pipefail
set +x

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
WORKING_DIR="${BUILD_DIR:-${REPO_DIR}/build/rdma}"
PERF_DIR="${DRIVERS_DIR}/perftest"

. "${SCRIPT_DIR}/common.sh"

function print_usage()
{
    log_info ""
    log_info "Usage:"
    log_info "\t $0 [option]"
    log_info ""
    log_info "Options:"
    log_info "\tinstall"
    log_info "\t\t setup irdma driver and libfabric"
    log_info "\tinstall_perftest"
    log_info "\t\t install perftest"
    log_info "\tcheck"
    log_info "\t\t check environment"
    log_info "\tset_mtu <INTERFACE>"
    log_info "\t\t temporarily set MTU to 9000 on given interface"
    log_info "\tperftest <INTERFACE>"
    log_info "\t\t run perftest"
}

function install_perftest()
{
    log_info "Start of install_perftest method. Installing apt packages."
    apt-get update --fix-missing && \
    apt install -y jq libibverbs-dev librdmacm-dev libibumad-dev libpci-dev
    log_info "Install_perftest method. Installing apt packages successful."

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
    as_root make install

    log_error "Intel irdma: Could not make and/or install the perftest."
    popd || log_warning "Intel irdma: Could not popd (directory). Ignoring."
    log_info "End of install_perftest method. Finished installing perftest-24.07.0."
}

function check_roce() {
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
        log_error  "Must be run as root. Try running below command:"
        log_error  "sudo \"${BASH_SOURCE[0]}\""
        exit 1
    fi

    if [[ -f "${WORKING_DIR}" ]]; then
        log_error  "Can't create rdma directory because of the rdma file ${WORKING_DIR}"
        exit 1
    fi
    mkdir -p "${WORKING_DIR}" "${PERF_DIR}"

    if [[ "$1" == "install" ]]; then
        rm -rf "${WORKING_DIR}" && mkdir -p "${WORKING_DIR}" && \
        build_install_and_config_irdma_drivers
        lib_install_fabrics
        log_warning "For changes to take place, an OS reboot is required."
    elif [[ "$1" == "install_perftest" ]]; then
        rm -rf "${WORKING_DIR}" && \
        mkdir -p "${WORKING_DIR}" && \
        install_perftest
    elif [[ "$1" == "check" ]]; then
        check_roce
        check_mtu
    elif [[ "$1" == "set_mtu" ]] && [[ -n "$2" ]]; then
        set_mtu "$2"
        check_mtu
    elif [[ "$1" == "perftest" ]] && [[ -n "$2" ]]; then
        run_perftest "${2}"
    else
        print_usage
    fi
fi
