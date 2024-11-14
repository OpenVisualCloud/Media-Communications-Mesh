#!/usr/bin/env bash

set -eEo pipefail
set +x

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
WORKING_DIR="${BUILD_DIR:-${REPO_DIR}/build/rdma}"

. "${SCRIPT_DIR}/setup_build_env.sh"

function install_perftest() {
    prompt "Start of install_perftest method. Installing apt packages."
    apt update
    apt install -y jq libibverbs-dev librdmacm-dev libibumad-dev libpci-dev
    prompt "Install_perftest method. Installing apt packages successful."

    prompt "Install_perftest method. Downloading and installing perftest-24.07.0."
    wget_download_strip_unpack "${PERF_REPO}" "${PERF_DIR}"

    pushd "${PERF_DIR}"
    ./autogen.sh
    ./configure
    make -j "${NPROC}"
    make install
    popd
    prompt "End of install_perftest method. Finished installing perftest-24.07.0."
}

function check_roce() {
    prompt -n "Checking RoCE... "
    roce_ena_val=$(cat /sys/module/irdma/parameters/roce_ena)
    if [[ "$roce_ena_val" != "1" ]] && [[ "$roce_ena_val" != "65535" ]]; then
        error "FAIL: RoCE disabled"
        exit 1
    fi
    prompt "OK"
}

function check_mtu() {
    prompt "Checking MTU..."
    rdma_devices=()
    rdma_links="$(rdma link -j | jq -r '.[] | select(.physical_state == "LINK_UP") | .netdev')"
    mapfile -t rdma_devices <<< "${rdma_links}"
    if [[ "${#rdma_devices[@]}" == "0" ]]; then
        error "FAIL: No RDMA devices detected"
        exit 1
    fi
    fail=0
    for device in "${rdma_devices[@]}"; do
        mtu=$(ip addr | grep "$device:" | cut -d " " -f 5)
        if [[ "$mtu" == "9000" ]]; then
            prompt "* $device OK"
        else
            error "* $device FAIL: MTU = $mtu (9000 is recommended)"
            fail=1
        fi
    done
    if [[ $fail -eq 1 ]]; then
        exit 1
    fi
}

function set_mtu() {
    prompt -n "Setting MTU to 9000 on $1... "
    ip link set dev "${1}" mtu 9000
    prompt "DONE"
}

function run_perftest() {
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
    prompt "perftest is running. Waiting 60 s..."
    wait $server_pid
    wait $client_pid
    prompt "perftest completed. See results in $WORKING_DIR/perftest_server.log and ${WORKING_DIR}/perftest_client.log"
    ip addr del "${network_address}/${network_mask}" dev "${interface_name}"
}

# Allow sourcing of the script.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]
then
    if [ "${EUID}" != "0" ]; then
        error "Must be run as root. Try running bellow command:"
        error "sudo \"${BASH_SOURCE[0]}\""
        exit 1
    fi

    if [[ -f "${WORKING_DIR}" ]]; then
        error "Can't create rdma directory because of the rdma file ${WORKING_DIR}"
        exit 1
    fi
    mkdir -p "${WORKING_DIR}" "${PERF_DIR}"

    if [[ "$1" == "install" ]]; then
    rm -rf "${WORKING_DIR}" && \
    mkdir -p "${WORKING_DIR}" && \
    build_install_and_config_irdma_drivers
    lib_install_fabrics
    warning "For changes to take place, an OS reboot is required."
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
        prompt "Usage:"
        prompt "\t $0 [option]"
        prompt ""
        prompt "Options:"
        prompt "\tinstall"
        prompt "\t\t setup irdma driver and libfabric"
        prompt "\tinstall_perftest"
        prompt "\t\t install perftest"
        prompt "\tcheck"
        prompt "\t\t check environment"
        prompt "\tset_mtu <INTERFACE>"
        prompt "\t\t temporarily set MTU to 9000 on given interface"
        prompt "\tperftest <INTERFACE>"
        prompt "\t\t run perftest"
    fi
fi
