#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

function get_next_free_ip()
{
    ip_last_octet="$(echo $1 | cut -d'.' -f4)"
    iface_old_ip="$1"
    while [ ! -z "$iface_old_ip" ]; do
        ip_last_octet="$((ip_last_octet+1))"
        iface_new_ip="$(echo $1 | cut -d'.' -f1-3).$ip_last_octet"
        iface_old_ip=$(ip -json a | jq ".[].addr_info[] | select(.family==\"inet\") | select(.local==\"$iface_new_ip\").local")
    done
    echo "${iface_new_ip}"
}

function run_test_af_xdp()
{
    iface_name="$($nicctl list all | grep $nic_pf | cut -f6)"
    iface_ip="$(ip -json a show $iface_name | jq '.[0].addr_info[] | select(.family=="inet").local' -r)"
    if [ -z $iface_ip ]; then
        iface_ip="$(get_next_free_ip "192.168.96.0")"
        iface_new_ip="$(get_next_free_ip "${iface_ip}")"
        ip address add "${iface_ip}/22" dev "${iface_name}" &>/dev/null
        ip_created="${iface_ip}"
    else
        ip_last_octet="$(echo $iface_ip | cut -d'.' -f4)"
        iface_new_ip="$(echo $iface_ip | cut -d'.' -f1-3).$((ip_last_octet+1))"
        ip_created=""
    fi

    run_test_af_xdp_rx &&
    run_test_af_xdp_tx
    local error=$?

    if [ ! -z "${ip_created}" ]; then
        ip address del "${ip_created}/22" dev "${iface_name}" &>/dev/null
    fi
    return $error
}

function run_test_af_xdp_rx()
{
    mkdir -p "$out_dir"

    info "Connection type: af_xdp af_xdp_rx"
    info "iface_name: $iface_name"
    info "iface_ip: $iface_ip"
    info "iface_new_ip: $iface_new_ip"
    info "ip_created: $ip_created"

    info "Starting MtlManager"
    local mtl_manager_cmd="MtlManager"
    run_in_background "$mtl_manager_cmd" "$mtl_manager_out"
    mtl_manager_pid=$!

    sleep 1

    info "Starting Tx side media_proxy"
    local tx_media_proxy_cmd="media_proxy -d $tx_vf_bdf -i $iface_new_ip -t 8002"
    run_in_background "$bin_dir/$tx_media_proxy_cmd" "$tx_media_proxy_out"
    tx_media_proxy_pid=$!

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -d native_af_xdp:$iface_name -i $iface_ip -t 8003"
    run_in_background "$bin_dir/$rx_media_proxy_cmd" "$rx_media_proxy_out"
    rx_media_proxy_pid=$!

    sleep 1

    info "Starting recver_app"
    export MCM_MEDIA_PROXY_PORT=8002
    local recver_app_cmd="recver_app -r $iface_ip -t st20 -w $width -h $height -f $fps -x $pixel_format -b $output_file -o auto"
    run_in_background "$bin_dir/$recver_app_cmd" "$recver_app_out"
    recver_app_pid=$!

    sleep 1

    info "Starting sender_app"
    export MCM_MEDIA_PROXY_PORT=8003
    local sender_app_cmd="sender_app -s $iface_new_ip -t st20 -w $width -h $height -f $fps -x $pixel_format -b $input_file -n $frames_number -o auto"
    run_in_background "$bin_dir/$sender_app_cmd" "$sender_app_out"
    sender_app_pid=$!

    wait_completion "$wait_interval"
    local timeout=$?

    sleep 1

    shutdown_apps
    kill "$mtl_manager_pid" &>/dev/null

    sleep 1

    check_results
    local error=$?

    info "Cleanup"
    cleanup $error

    return $(($error || $timeout))
}

function run_test_af_xdp_tx()
{
    mkdir -p "$out_dir"

    info "Connection type: af_xdp af_xdp_tx"
    info "iface_name: $iface_name"
    info "iface_ip: $iface_ip"
    info "iface_new_ip: $iface_new_ip"

    info "Starting MtlManager"
    local mtl_manager_cmd="MtlManager"
    run_in_background "$mtl_manager_cmd" "$mtl_manager_out"
    mtl_manager_pid=$!

    sleep 1

    info "Starting Tx side media_proxy"
    local tx_media_proxy_cmd="media_proxy -d native_af_xdp:$iface_name -i $iface_ip -t 8002"
    run_in_background "$bin_dir/$tx_media_proxy_cmd" "$tx_media_proxy_out"
    tx_media_proxy_pid=$!

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -d $rx_vf_bdf -i $iface_new_ip -t 8003"
    run_in_background "$bin_dir/$rx_media_proxy_cmd" "$rx_media_proxy_out"
    rx_media_proxy_pid=$!

    sleep 1

    info "Starting recver_app"
    export MCM_MEDIA_PROXY_PORT=8003
    local recver_app_cmd="recver_app -r $iface_ip -t st20 -w $width -h $height -f $fps -x $pixel_format -b $output_file -o auto"
    run_in_background "$bin_dir/$recver_app_cmd" "$recver_app_out"
    recver_app_pid=$!

    sleep 1

    info "Starting sender_app"
    export MCM_MEDIA_PROXY_PORT=8002
    local sender_app_cmd="sender_app -s $iface_new_ip -t st20 -w $width -h $height -f $fps -x $pixel_format -b $input_file -n $frames_number -o auto"
    run_in_background "$bin_dir/$sender_app_cmd" "$sender_app_out"
    sender_app_pid=$!

    wait_completion "$wait_interval"
    local timeout=$?

    sleep 1

    shutdown_apps
    kill "$mtl_manager_pid" &>/dev/null

    sleep 1

    check_results
    local error=$?

    info "Cleanup"
    cleanup $error

    return $(($error || $timeout))
}
