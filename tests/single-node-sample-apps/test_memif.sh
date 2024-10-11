#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

function run_test_memif_rx() {
    # Notice: media_proxy does not take part in the direct memif communication.
    # However, we start Tx and Rx media_proxy to check if they print expected
    # messages and do not produce any errors.

    # Notice: sender_app should be started before recver_app. Currently, this is
    # the only successful scenario.
    mkdir -p "${out_dir}"
    export socket_path="/run/mcm/mcm_rx_memif.sock"

    info "Connection type: MEMIF libmemif_rx"

    info "Starting Tx side media_proxy"
    local tx_media_proxy_cmd="media_proxy -d ${tx_vf_bdf} -i 192.168.96.1 -t 8002"
    run_in_background "${bin_dir}/${tx_media_proxy_cmd}" "${tx_media_proxy_out}"
    tx_media_proxy_pid="$!"

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -d $rx_vf_bdf -i 192.168.96.2 -t 8003"
    run_in_background "${bin_dir}/${rx_media_proxy_cmd}" "${rx_media_proxy_out}"
    rx_media_proxy_pid="$!"

    sleep 1

    info "Starting sender_app"
    export MCM_MEDIA_PROXY_PORT=8002
    local sender_app_cmd="sender_app -s 192.168.96.2 -t st20 -w ${width} -h ${height} -f ${fps} -x ${pixel_format} -b ${input_file} -n ${frames_number} -o memif --socketpath=${socket_path}"
    run_in_background "${bin_dir}/${sender_app_cmd}" "${sender_app_out}"
    sender_app_pid="$!"

    sleep 1

    info "Starting memif_test_rx"
    local recver_app_cmd="memif_test_rx -w ${width} -h ${height} -f ${output_file} -s ${socket_path}"
    run_in_background "${bin_dir}/${recver_app_cmd}" "${recver_app_out}"
    recver_app_pid="$!"

    wait_completion "${wait_interval}"
    local timeout="$?"

    sleep 1

    shutdown_apps

    sleep 1

    check_results
    local error="$?"

    info "Cleanup"
    cleanup

    return $((error || timeout))
}

function run_test_memif_tx() {
    # Notice: media_proxy does not take part in the direct memif communication.
    # However, we start Tx and Rx media_proxy to check if they print expected
    # messages and do not produce any errors.

    # Notice: sender_app should be started before recver_app. Currently, this is
    # the only successful scenario.
    mkdir -p "$out_dir"
    export socket_path="/run/mcm/mcm_rx_memif.sock"

    info "Connection type: MEMIF libmemif_tx"

    info "Starting Tx side media_proxy"
    local tx_media_proxy_cmd="media_proxy -d $tx_vf_bdf -i 192.168.96.1 -t 8002"
    run_in_background "$bin_dir/$tx_media_proxy_cmd" "$tx_media_proxy_out"
    tx_media_proxy_pid="$!"

    sleep 1

    info "Starting Rx side media_proxy"
    local rx_media_proxy_cmd="media_proxy -d $rx_vf_bdf -i 192.168.96.2 -t 8003"
    run_in_background "$bin_dir/$rx_media_proxy_cmd" "$rx_media_proxy_out"
    rx_media_proxy_pid="$!"

    sleep 1

    info "Starting memif_test_tx"
    local sender_app_cmd="memif_test_tx -m -w $width -h $height -f $input_file -s $socket_path"
    run_in_background "$bin_dir/$sender_app_cmd" "$sender_app_out"
    sender_app_pid="$!"

    sleep 1

    info "Starting recver_app"
    export MCM_MEDIA_PROXY_PORT=8003
    local recver_app_cmd="recver_app -r 192.168.96.1 -t st20 -w $width -h $height -f $fps -x $pixel_format -b $output_file -o memif --socketpath=$socket_path"
    run_in_background "$bin_dir/$recver_app_cmd" "$recver_app_out"
    recver_app_pid="$!"

    wait_completion "$wait_interval"
    local timeout="$?"

    sleep 1

    shutdown_apps

    sleep 1

    check_results
    local error="$?"

    info "Cleanup"
    cleanup

    return $((error || timeout))
}
