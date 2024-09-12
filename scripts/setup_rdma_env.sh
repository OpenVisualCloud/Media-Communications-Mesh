#/usr/bin/env bash

if [[ ! -d rdma ]]; then
    mkdir rdma
fi
cd rdma

install_ice() {
    echo "Installing ice driver..."
    wget https://downloadmirror.intel.com/832286/ice-1.15.4.tar.gz -O ice.tar.gz
    tar -xf ice.tar.gz
    cd ice-*/src
    sudo make install
    cd ../..
}

install_irdma() {
    echo "Installing irdma driver..."
    wget https://downloadmirror.intel.com/832291/irdma-1.15.11.tgz -O irdma.tar.gz
    tar -xf irdma.tar.gz
    cd irdma-*
    sudo ./build.sh
    cd ..
    modprobe irdma
}

configure_irdma() {
    echo "Configuring irdma driver..."

    # enable RoCE
    roce_ena_val=$(grep "options irdma roce_ena=" /etc/modprobe.d/irdma.conf, cut -d "=" -f 2)
    if [[ -z "$roce_ena_val" ]]; then
        echo "options irdma roce_ena=1" | sudo tee -a /etc/modprobe.d/irdma.conf
        sudo dracut -f
    elif [[ "$roce_ena_val" != "1" ]]; then
        sed -i '/options irdma roce_ena=/s/roce_ena=[0-9]*/roce_ena=1/' a.conf
        sudo dracut -f
    fi

    # increase irdma Queue Pair limit
    limits_sel_val=$(grep "options irdma limits_sel=" /etc/modprobe.d/irdma.conf, cut -d "=" -f 2)
    if [[ -z "$limits_sel_val" ]]; then
        echo "options irdma limits_sel=5" | sudo tee -a /etc/modprobe.d/irdma.conf
        sudo dracut -f
    elif [[ "$limits_sel_val" != "1" ]]; then
        sed -i '/options irdma limits_sel=/s/limits_sel=[0-9]*/limits_sel=5/' a.conf
        sudo dracut -f
    fi
}

install_perftest() {
    echo "Installing perftest dependencies..."
    sudo apt update
    sudo apt install -y libibverbs-dev librdmacm-dev libibumad-dev libpci-dev
    echo "Installing perftest..."
    git clone https://github.com/linux-rdma/perftest.git perftest
    cd perftest
    ./autogen.sh
    ./configure
    make
    sudo make install
    cd ..
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
    sudo ip addr add 192.168.255.255/24 dev $1
    taskset -c 1 ib_write_bw --qp=4 --report_gbit -D 60 --tos 96 -R &> perftest_server.log &
    server_pid=$!
    ib_write_bw 192.168.255.255 --qp=4 --report_gbit -D 60 --tos 96 -R &> perftest_client.log &
    client_pid=$!
    echo "perftest is running. Waiting 60 s..."
    wait $server_pid
    wait $client_pid
    echo "perftest completed. See results in rdma/perftest_server.log and rdma/perftest_client.log"
    sudo ip addr del 192.168.255.255/24 dev $1
}

if [[ "$1" == "install" ]]; then
    rm -rf ../rdma/*
    # install_ice
    install_irdma
    configure_irdma
    install_perftest
    echo "Reboot required"
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
    echo -e "$0 install" "\n\t" "setup environment\n"
    echo -e "$0 check" "\n\t" "check environment\n"
    echo -e "$0 set_mtu <INTERFACE>" "\n\t" "temporarily set MTU to 9000 on given interface\n"
    echo -e "$0 perftest <INTERFACE>" "\n\t" "run perftest\n"
fi
