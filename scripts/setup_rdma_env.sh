#!/usr/bin/env bash

set -exo pipefail

SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
REPO_DIR="$(readlink -f "${SCRIPT_DIR}/..")"
WORKING_DIR="${BUILD_DIR:-${REPO_DIR}/build/rdma}"

. "${SCRIPT_DIR}/common.sh"

IRDMA_DMID="832291"
IRDMA_VER="1.15.11"

if [[ -f $WORKING_DIR ]]; then
    error "Can't create rdma directory because of the rdma file $WORKING_DIR"
    exit 1
fi

mkdir -p "$WORKING_DIR"

install_irdma() {
    echo "Installing irdma driver..."
    pushd "$WORKING_DIR"
    wget "https://downloadmirror.intel.com/${IRDMA_DMID}/irdma-${IRDMA_VER}.tgz" -O ./irdma.tar.gz
    tar -xf ./irdma.tar.gz
    pushd "./irdma-${IRDMA_VER}"
    sudo ./build.sh
    popd
    popd
    modprobe irdma
}

install_perftest() {
    echo "Installing perftest dependencies..."
    sudo apt update
    sudo apt install -y libibverbs-dev librdmacm-dev libibumad-dev libpci-dev
    echo "Installing perftest..."
    pushd "$WORKING_DIR"
    wget https://github.com/linux-rdma/perftest/releases/download/24.07.0-0.44/perftest-24.07.0-0.44.g57725f2.tar.gz -O perftest.tar.gz
    tar -xf perftest.tar.gz
    pushd ./perftest-24.07.0
    ./autogen.sh
    ./configure
    make -j$(nproc)
    sudo make install
    popd
    popd
}

install_libfabric() {
    echo "Installing libfabric from the source..."
    pushd "$WORKING_DIR"
    git clone --depth 1 --branch v1.22.0 https://github.com/ofiwg/libfabric libfabric
    pushd libfabric
    ./autogen.sh
    ./configure --enable-verbs
    make -j$(nproc)
    sudo make install
    sudo ldconfig
    popd
    popd
}

check_roce() {
    echo -n "Checking RoCE... "
    roce_ena_val=$(cat /sys/module/irdma/parameters/roce_ena)
    if [[ "$roce_ena_val" != "1" ]] && [[ "$roce_ena_val" != "65535" ]]; then
        echo "FAIL: RoCE disabled"
        exit 1
    fi
    echo "OK"
}

check_mtu() {
    echo "Checking MTU..."
    rdma_devices=($(rdma link | grep "LINK_UP" | cut -d " " -f 8))
    if [[ "${#rdma_devices[@]}" == "0" ]]; then
        echo "FAIL: No RDMA devices detected"
        exit 1
    fi
    fail=0
    for device in "${rdma_devices[@]}"; do
        mtu=$(ip addr | grep "$device:" | cut -d " " -f 5)
        if [[ "$mtu" == "9000" ]]; then
            echo "* $device OK"
        else
            echo "* $device FAIL: MTU = $mtu (9000 is recommended)"
            fail=1
        fi
    done
    if [[ $fail -eq 1 ]]; then
        exit 1
    fi
}

set_mtu() {
    echo -n "Setting MTU to 9000 on $1... "
    sudo ip link set dev $1 mtu 9000
    echo "DONE"
}

run_perftest() {
    # Usage: run_perftest <interface_name> [network_address] [network_mask]
    interface_name="$1"
    network_address="${2:-192.168.255.255}"
    network_mask="${3:-24}"
    sudo ip addr add ${network_address}/${network_mask} dev ${interface_name}
    taskset -c 1 ib_write_bw --qp=4 --report_gbit -D 60 --tos 96 -R &> "$WORKING_DIR"/perftest_server.log &
    server_pid=$!
    ib_write_bw ${network_address} --qp=4 --report_gbit -D 60 --tos 96 -R &> "$WORKING_DIR"/perftest_client.log &
    client_pid=$!
    echo "perftest is running. Waiting 60 s..."
    wait $server_pid
    wait $client_pid
    echo "perftest completed. See results in $WORKING_DIR/perftest_server.log and $WORKING_DIR/perftest_client.log"
    sudo ip addr del ${network_address}/${network_mask} dev ${interface_name}
}

if [[ "$1" == "install" ]]; then
    rm -rf "$WORKING_DIR"/*
    install_irdma
    config_intel_rdma_driver
    install_libfabric
    echo "Reboot required"
elif [[ "$1" == "install_perftest" ]]; then
    rm -rf "$WORKING_DIR"/*
    install_perftest
elif [[ "$1" == "check" ]]; then
    check_roce
    check_mtu
elif [[ "$1" == "set_mtu" ]] && [[ -n "$2" ]]; then
    set_mtu "$2"
    check_mtu
elif [[ "$1" == "perftest" ]] && [[ -n "$2" ]]; then
    run_perftest $2
else
    echo -e "Usage\n"
    echo -e "$0 install" "\n\t" "setup irdma driver and libfabric\n"
    echo -e "$0 install_perftest" "\n\t" "install perftest\n"
    echo -e "$0 check" "\n\t" "check environment\n"
    echo -e "$0 set_mtu <INTERFACE>" "\n\t" "temporarily set MTU to 9000 on given interface\n"
    echo -e "$0 perftest <INTERFACE>" "\n\t" "run perftest\n"
fi
