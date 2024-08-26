#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

function run_test_af_xdp_rx()
{
    return 0

    iface_name="$($nicctl list all | grep $nic_pf | cut -f6)"
    iface_ip="$(ip -json a show $iface_name | jq '.[0].addr_info[0].local' -r)"
    ip_last_octet="$(echo $iface_ip | cut -d'.' -f4)"
    iface_new_ip="$(echo $iface_ip | cut -d'.' -f1-3).$((ip_last_octet+1))"

    mkdir -p "$out_dir"

    info "Connection type: af_xdp af_xdp_rx"
    info "iface_name: $iface_name"
    info "iface_ip: $iface_ip"
    info "iface_new_ip: $iface_new_ip"

    info "Starting MtlManager"
    local mtl_manager_cmd="MtlManager"
    run_in_background "$mtl_manager_cmd" "$mtl_manager_out"
    mtl_manager_pid=$!

    sleep 1

    info "Starting Tx side media_proxy"
    local tx_media_proxy_cmd="media_proxy -d $tx_vf_bdf -i $iface_new_ip -t 8002"
    info "tx_media_proxy_cmd=$tx_media_proxy_cmd"
    run_in_background "$bin_dir/$tx_media_proxy_cmd" "$tx_media_proxy_out"
    tx_media_proxy_pid=$!

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -d native_af_xdp:$iface_name -i $iface_ip -t 8003"
    info "rx_media_proxy_cmd=$rx_media_proxy_cmd"
    run_in_background "$bin_dir/$rx_media_proxy_cmd" "$rx_media_proxy_out"
    rx_media_proxy_pid=$!

    sleep 1

    info "Starting recver_app"
    export MCM_MEDIA_PROXY_PORT=8003
    local recver_app_cmd="recver_app -r $iface_new_ip -t st20 -w $width -h $height -f $fps -x $pixel_format -b $output_file -o auto"
    info "recver_app_cmd=$recver_app_cmd"
    run_in_background "$bin_dir/$recver_app_cmd" "$recver_app_out"
    recver_app_pid=$!

    sleep 1

    info "Starting sender_app"
    export MCM_MEDIA_PROXY_PORT=8002
    local sender_app_cmd="sender_app -s $iface_ip -t st20 -w $width -h $height -f $fps -x $pixel_format -b $input_file -n $frames_number -o auto"
    info "sender_app_cmd=$sender_app_cmd"
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
    cleanup

    return $(($error || $timeout))
}

function run_test_af_xdp_tx()
{
    iface_name="$($nicctl list all | grep $nic_pf | cut -f6)"
    iface_ip="$(ip -json a show $iface_name | jq '.[0].addr_info[0].local' -r)"
    ip_last_octet="$(echo $iface_ip | cut -d'.' -f4)"
    iface_new_ip="$(echo $iface_ip | cut -d'.' -f1-3).$((ip_last_octet+1))"

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
    info "tx_media_proxy_cmd=$tx_media_proxy_cmd"
    run_in_background "$bin_dir/$tx_media_proxy_cmd" "$tx_media_proxy_out"
    tx_media_proxy_pid=$!

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -d $rx_vf_bdf -i $iface_new_ip -t 8003"
    info "rx_media_proxy_cmd=$rx_media_proxy_cmd"
    run_in_background "$bin_dir/$rx_media_proxy_cmd" "$rx_media_proxy_out"
    rx_media_proxy_pid=$!

    sleep 1

    info "Starting recver_app"
    export MCM_MEDIA_PROXY_PORT=8003
    local recver_app_cmd="recver_app -r $iface_ip -t st20 -w $width -h $height -f $fps -x $pixel_format -b $output_file -o auto"
    info "recver_app_cmd=$recver_app_cmd"
    run_in_background "$bin_dir/$recver_app_cmd" "$recver_app_out"
    recver_app_pid=$!

    sleep 1

    info "Starting sender_app"
    export MCM_MEDIA_PROXY_PORT=8002
    local sender_app_cmd="sender_app -s $iface_new_ip -t st20 -w $width -h $height -f $fps -x $pixel_format -b $input_file -n $frames_number -o auto"
    info "sender_app_cmd=$sender_app_cmd"
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
    cleanup

    return $(($error || $timeout))
}
