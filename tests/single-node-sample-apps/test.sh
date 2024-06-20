#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

# Directories
script_dir="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
bin_dir="$script_dir/../../out/bin"
out_dir="$script_dir/out"
mtl_dir="$script_dir/../../../Media-Transport-Library"

# Test configuration
test_option="$1"
nic_pf="$2"
tx_vf_number=0
rx_vf_number=1

# Media file names
input_file="$(realpath $3)"
output_file="$out_dir/out_rx.bin"

# Video parameters
duration="${4:-10}"
frames_number="${5:-300}"
width="${6:-640}"
height="${7:-360}"
fps="${8:-60}"
pixel_format="${9:-yuv422p10le}"

# Test configuration (cont'd)
wait_interval=$((duration + 5))

# Stdout/stderr forwarded output file names
tx_media_proxy_out="$out_dir/out_tx_media_proxy.txt"
rx_media_proxy_out="$out_dir/out_rx_media_proxy.txt"
sender_app_out="$out_dir/out_sender_app.txt"
recver_app_out="$out_dir/out_recver_app.txt"

# Number of test repetitions
repeat_number=3

function message() {
    local type="$1"
    shift
    local highlight_on="\e[38;05;33m"
    local highlight_off="\e[m"
    local timestamp="$(date +'%Y-%m-%d %H:%M:%S')"
    echo -e "${highlight_on}$timestamp $type${highlight_off} $*"
}

function info() {
    local highlight_on='\e[38;05;45m';
    message "${highlight_on}[INFO]" "$*"
}

function error() {
    local highlight_on='\e[38;05;9m';
    message "${highlight_on}[ERRO]" "$*"
}

function cleanup() {
    rm -rf "$out_dir" &>/dev/null
}

function run_in_background() {
    # 1st argument is a command with arguments to be run in background
    # 2nd argument is a file for stdout/stderr to be redirected to
    # Returns PID of a spawned process
    stdbuf -o0 -e0 $1 &>"$2" &
}

function wait_text() {
    # 1st argument is a timeout interval
    # 2nd argument is a file to be monitored
    # 3rd argument is a text to be looked for in the file
    timeout $1 grep -q "$3" <(tail -f "$2")
    [ $? -eq 124 ] && error "timeout occurred" && return 1
    return 0
}

function wait_completion() {
    # 1st argument is a timeout interval
    info "Waiting for transmission to complete (interval ${1}s)"
    timeout $1 tail --pid=$sender_app_pid --pid=$recver_app_pid -f /dev/null
    [ $? -eq 124 ] && error "timeout occurred" && return 1
    info "Transmission completed" 
    return 0
}

function shutdown_apps() {
    info "Shutting down apps"

    kill "$sender_app_pid" &>/dev/null
    kill "$recver_app_pid" &>/dev/null

    kill "$tx_media_proxy_pid" &>/dev/null
    kill "$rx_media_proxy_pid" &>/dev/null
}

function check_results() {
    local error=0

    # Compare input and output files
    diff "$input_file" "$output_file"
    if [ $? -ne 0 ]; then
        error=1
        error "An error occurred while comparing input and output files"
        info "Output file size: $(stat -c%s $output_file) byte(s)"
    else
        info "Input and output files are identical"
    fi

    # Check output for errors
    grep "error\|fail\|invalid" -i "$sender_app_out" /dev/null
    [ $? -eq 0 ] && error=1 && error "Error signature(s) found in the sender_app output"

    grep "error\|fail\|invalid" -i "$recver_app_out" /dev/null
    [ $? -eq 0 ] && error=1 && error "Error signature(s) found in the recver_app output"

    grep "error" -i "$tx_media_proxy_out" /dev/null
    [ $? -eq 0 ] && error=1 && error "Error signature(s) found in the Tx media_proxy output"

    grep "error" -i "$rx_media_proxy_out" /dev/null
    [ $? -eq 0 ] && error=1 && error "Error signature(s) found in the Rx media_proxy output"

    # Check output for expected messages
    grep "TCP Server listening" -iq "$tx_media_proxy_out"
    [ $? -ne 0 ] && error=1 && error "Expected message not found in the Tx media_proxy output"

    grep "TCP Server listening" -iq "$rx_media_proxy_out"
    [ $? -ne 0 ] && error=1 && error "Expected message not found in the Rx media_proxy output"

    return $error
}

function get_vf_bdf() {
    local pf="$1"
    local vf="$2"
    local params=$(cat "/sys/bus/pci/devices/$pf/virtfn$vf/uevent")
    local regex="PCI_SLOT_NAME=([0-9a-f]{4}:[0-9a-f]{2,4}:[0-9a-f]{2}\.[0-9a-f])"
    [[ ! $params =~ $regex ]] && error "Cannot get VF $vf bus device function" && return 1
    echo "${BASH_REMATCH[1]}"
    return 0
}

function initialize_nic() {
    local pf="$1"
    local tx_vf="$2"
    local rx_vf="$3"

    info "Checking PF $pf status"

    # Check if PF exists
    lspci -D | grep $pf &>/dev/null
    [ $? -ne 0 ] && error "PF not found" && return 1

    # Get network interface name of PF
    local description=$(lshw -c network -businfo -quiet | grep "$pf")
    local regex="pci@$pf[[:space:]]+([a-zA-Z0-9]+)"
    [[ ! $description =~ $regex ]] && error "Cannot get network interface name" && return 1

    # Check if network interface is up
    local iface="${BASH_REMATCH[1]}"
    [[ ! $(ip link show $iface) =~ "state UP" ]] && error "Network interface $iface link is down" && return 1
    info "Network interface $iface link is up"

    # Disable VFs
    info "Disabling VFs on $pf"
    $mtl_dir/script/nicctl.sh disable_vf "$pf" 1>/dev/null
    [ $? -ne 0 ] && error "VF disabling failed" && return 1

    sleep 1

    # Create VFs
    info "Creating VFs on $pf"
    $mtl_dir/script/nicctl.sh create_vf "$pf" 1>/dev/null
    [ $? -ne 0 ] && error "VF creation failed" && return 1

    # Discover VFs
    vfs_number=$(cat "/sys/bus/pci/devices/$pf/sriov_numvfs" 2>/dev/null)
    [ ! $? -eq 0 ] && error "No VFs available" && return 1

    [ "$vfs_number" -le "$tx_vf" ] || [ "$tx_vf" -lt 0 ] && error "Wrong Tx VF index $tx_vf. Total VFs available: $vfs_number." && return 1
    [ "$vfs_number" -le "$rx_vf" ] || [ "$rx_vf" -lt 0 ] && error "Wrong Rx VF index $rx_vf. Total VFs available: $vfs_number." && return 1
    [ "$tx_vf" -eq "$rx_vf" ] && error "Rx and Tx VF indexes should differ" && return 1

    tx_vf_bdf=$(get_vf_bdf $pf $tx_vf)
    [ $? -ne 0 ] && return 1

    info "Tx VF$tx_vf BDF: $tx_vf_bdf"

    rx_vf_bdf=$(get_vf_bdf $pf $rx_vf)
    [ $? -ne 0 ] && return 1

    info "Rx VF$rx_vf BDF: $rx_vf_bdf"

    return 0
}

function deinitialize_nic() {
    local pf="$1"

    # Disable VFs
    info "Disabling VFs on $pf"
    $mtl_dir/script/nicctl.sh disable_vf "$pf" 1>/dev/null
    [ $? -ne 0 ] && error "VF disabling failed" && return 1

    return 0
}

function run_test_memif() {
    # Notice: media_proxy does not take part in the direct memif communication.
    # However, we start Tx and Rx media_proxy to check if they print expected
    # messages and do not produce any errors.

    # Notice: sender_app should be started before recver_app. Currently, this is
    # the only successful scenario.

    info "Connection type: MEMIF"

    info "Starting Tx side media_proxy"
    local tx_media_proxy_cmd="media_proxy -d $tx_vf_bdf -i 192.168.96.1 -t 8002"
    run_in_background "$bin_dir/$tx_media_proxy_cmd" "$tx_media_proxy_out"
    tx_media_proxy_pid=$!

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -d $rx_vf_bdf -i 192.168.96.2 -t 8003"
    run_in_background "$bin_dir/$rx_media_proxy_cmd" "$rx_media_proxy_out"
    rx_media_proxy_pid=$!

    sleep 1

    info "Starting sender_app"
    export MCM_MEDIA_PROXY_PORT=8002
    local sender_app_cmd="sender_app -s 192.168.96.2 -t st20 -w $width -h $height -f $fps -x $pixel_format -b $input_file -n $frames_number -o memif"
    run_in_background "$bin_dir/$sender_app_cmd" "$sender_app_out"
    sender_app_pid=$!

    sleep 1

    info "Starting recver_app"
    export MCM_MEDIA_PROXY_PORT=8003
    local recver_app_cmd="recver_app -r 192.168.96.1 -t st20 -w $width -h $height -f $fps -x $pixel_format -b $output_file -o memif"
    run_in_background "$bin_dir/$recver_app_cmd" "$recver_app_out"
    recver_app_pid=$!

    wait_completion "$wait_interval"
    local timeout=$?

    sleep 1

    shutdown_apps

    sleep 1

    check_results
    local error=$?

    info "Cleanup"
    cleanup

    return $(($error || $timeout))
}

function run_test_stXX() {
    # 1st argument is the connection type ('st20' or 'st22')

    # Notice: recver_app should be started before sender_app. Currently, this is
    # the only successful scenario.

    [[ "$1" != "st20" && "$1" != "st22" ]] && error "Unknown connection type argument" && return 1

    info "Connection type: ${1^^}"

    info "Starting Tx side media_proxy"
    local tx_media_proxy_cmd="media_proxy -d $tx_vf_bdf -i 192.168.96.1 -t 8002"
    run_in_background "$bin_dir/$tx_media_proxy_cmd" "$tx_media_proxy_out"
    tx_media_proxy_pid=$!

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -d $rx_vf_bdf -i 192.168.96.2 -t 8003"
    run_in_background "$bin_dir/$rx_media_proxy_cmd" "$rx_media_proxy_out"
    rx_media_proxy_pid=$!

    sleep 1

    info "Starting recver_app"
    export MCM_MEDIA_PROXY_PORT=8003
    local recver_app_cmd="recver_app -r 192.168.96.1 -t $1 -w $width -h $height -f $fps -x $pixel_format -b $output_file -o auto"
    run_in_background "$bin_dir/$recver_app_cmd" "$recver_app_out"
    recver_app_pid=$!

    info "Waiting for recver_app to connect to Rx media_proxy"
    wait_text 10 $recver_app_out "Success connect to MCM media-proxy"
    local recver_app_timeout=$?
    [ $recver_app_timeout -eq 0 ] && info "Connection established"

    info "Starting sender_app"
    export MCM_MEDIA_PROXY_PORT=8002
    local sender_app_cmd="sender_app -s 192.168.96.2 -t $1 -w $width -h $height -f $fps -x $pixel_format -b $input_file -n $frames_number -o auto"
    run_in_background "$bin_dir/$sender_app_cmd" "$sender_app_out"
    sender_app_pid=$!

    info "Waiting for sender_app to connect to Tx media_proxy"
    wait_text 10 $sender_app_out "Success connect to MCM media-proxy"
    local sender_app_timeout=$?
    [ $sender_app_timeout -eq 0 ] && info "Connection established"

    wait_completion "$wait_interval"
    local timeout=$?

    sleep 1

    shutdown_apps

    check_results
    local error=$?

    info "Tx media_proxy stats"
    local regex="throughput ([0-9]*\.[0-9]*) Mb/s: [0-9]*\.[0-9]* Mb/s, cpu busy ([0-9]*\.[0-9]*)"
    while IFS= read -r line; do
        if [[ $line =~ $regex ]]; then
            info "  Throughput ${BASH_REMATCH[1]} Mb/s, CPU busy ${BASH_REMATCH[2]}"
        fi
    done < "$tx_media_proxy_out"

    info "Rx media_proxy stats"
    local regex="throughput ([0-9]*\.[0-9]*) Mb/s, cpu busy ([0-9]*\.[0-9]*)"
    while IFS= read -r line; do
        if [[ $line =~ $regex ]]; then
            info "  Throughput ${BASH_REMATCH[1]} Mb/s, CPU busy ${BASH_REMATCH[2]}"
        fi
    done < "$rx_media_proxy_out"

    info "Cleanup"
    cleanup

    return $(($error || $timeout || $recver_app_timeout || $sender_app_timeout))
}

info "Test MCM Tx/Rx for Single Node"
info "  Binary directory: $(realpath $bin_dir)"
info "  Output directory: $(realpath $out_dir)"
info "  Input file path: $input_file"
info "  Input file size: $(stat -c%s $input_file) byte(s)"
info "  Frame size: $width x $height, FPS: $fps"
info "  Pixel format: $pixel_format"
info "  Duration: ${duration}s"
info "  Frames number: $frames_number"
info "  Test option: $test_option"
info "  NIC PF: $nic_pf"

[ ! -f "$input_file" ] && error "Input file not found" && exit 1

info "Initial cleanup"
cleanup

initialize_nic "$nic_pf" "$tx_vf_number" "$rx_vf_number"
[ $? -ne 0 ] && error "NIC initialization failed" && exit 1

for i in $(seq 1 $repeat_number);
do
    info "--- Run #$i ---"

    mkdir -p "$out_dir"

    case $test_option in
        memif)
            run_test_memif
            ;;
        st20)
            run_test_stXX st20
            ;;
        st22)
            run_test_stXX st22
            ;;
        *)
            error "Unknown test option"
            exit 1
    esac

    [ $? -ne 0 ] && error "Test failed" && exit 1
done

deinitialize_nic "$nic_pf"

info "Test completed successfully"
