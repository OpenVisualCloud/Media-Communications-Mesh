#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

# Directories
script_dir="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
bin_dir="$script_dir/../../_build/bin"
out_dir="$script_dir/out"

# Media file names
input_file="$(realpath $1)"
output_file="$out_dir/out_rx.bin"

# Video parameters
duration="${2:-10}"
frames_number="${3:-300}"
width="${4:-640}"
height="${5:-360}"
pixel_format="${6:-yuv422p10le}"
rdma_iface_ip="${7:-127.0.0.1}"

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
    timeout $1 grep -q "$3" <(tail -n 100 -f "$2")
    [ $? -eq 124 ] && error "timeout occurred" && return 1
    return 0
}

function wait_completion() {
    # 1st argument is a timeout interval
    info "Waiting for both sender and receiver to complete (interval ${1}s)"
    local end_time=$((SECONDS + $1))

    while kill -0 $sender_app_pid 2>/dev/null && kill -0 $recver_app_pid 2>/dev/null; do
    if (( SECONDS >= end_time )); then
         error "timeout occurred"
         return 1
    fi
        sleep 1
    done

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



function run_test_rdma() {
    # Notice: recver_app should be started before sender_app. Currently, this is
    # the only successful scenario.

    info "Connection type: rdma"

    info "Starting Tx side media_proxy"
    local tx_media_proxy_cmd="media_proxy -t 8002"
    run_in_background "$bin_dir/$tx_media_proxy_cmd" "$tx_media_proxy_out"
    tx_media_proxy_pid="$!"

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -t 8003"
    run_in_background "$bin_dir/$rx_media_proxy_cmd" "$rx_media_proxy_out"
    rx_media_proxy_pid="$!"

    sleep 1

    info "Starting recver_app"
    export MCM_MEDIA_PROXY_PORT=8003
    local recver_app_cmd="recver_app -r $rdma_iface_ip -t rdma -w $width -h $height -x $pixel_format -b $output_file -o auto"
    run_in_background "$bin_dir/$recver_app_cmd" "$recver_app_out"
    recver_app_pid="$!"


    info "Starting sender_app"
    export MCM_MEDIA_PROXY_PORT=8002
    local sender_app_cmd="sender_app -s $rdma_iface_ip -t rdma -w $width -h $height -x $pixel_format -b $input_file -n $frames_number -o auto"
    run_in_background "$bin_dir/$sender_app_cmd" "$sender_app_out"
    sender_app_pid="$!"

    info "Waiting for recver_app to connect to Rx media_proxy"
    wait_text 10 $recver_app_out "Success connect to MCM media-proxy"
    local recver_app_timeout="$?"
    [ $recver_app_timeout -eq 0 ] && info "Connection established"

    info "Waiting for sender_app to connect to Tx media_proxy"
    wait_text 10 $sender_app_out "Success connect to MCM media-proxy"
    local sender_app_timeout="$?"
    [ $sender_app_timeout -eq 0 ] && info "Connection established"

    wait_completion "$wait_interval"
    local timeout="$?"

    sleep 1

    shutdown_apps

    check_results
    local error="$?"

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
info "  Frame size: $width x $height"
info "  Pixel format: $pixel_format"
info "  Duration: ${duration}s"
info "  Frames number: $frames_number"

[ ! -f "$input_file" ] && error "Input file not found" && exit 1

info "Initial cleanup"
cleanup

for i in $(seq 1 $repeat_number);
do
    info "--- Run #$i ---"

    mkdir -p "$out_dir"
    run_test_rdma

    [ $? -ne 0 ] && error "Test failed" && exit 1
done

info "Test completed successfully"
