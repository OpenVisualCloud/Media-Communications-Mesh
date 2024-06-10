#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail
SCRIPT_DIR="$(readlink -f "$(dirname -- "${BASH_SOURCE[0]}")")"
. "${SCRIPT_DIR}/../common.sh"

# Function to display execution step log
display_step_info() {
    step_num=$((step_num + 1))
    echo -e "\033[1;32m[Step $step_num] $1\033[0m"
}

# Default number of nodes
num_nodes=3
step_num=0

while getopts "n:h" opt; do
    case ${opt} in
        n)
            display_step_info "Setting number of nodes to $OPTARG"
            num_nodes=$OPTARG
            ;;
        h)
            echo "Usage: $0 [-n NUM_NODES]" >&2
            exit 0
            ;;
    esac
done

display_step_info "Starting the Minikube cluster with $num_nodes nodes"
minikube start --nodes $num_nodes --namespace mcm --mount-string="/var/run/imtl:/var/run/imtl" --mount

# Enable minikube addons
display_step_info "Enabling metrics-server addon"
minikube addons enable metrics-server

# Load docker images into cluster
display_step_info "Loading docker images into the cluster"
minikube image load mcm/media-proxy:latest
minikube image load mcm/sample-app:latest

# Create an array for node names
nodearr=()
for ((i=2; i<=$num_nodes; i++)); do
  nodearr+=("minikube-m$(printf %02d $i)")
done

# Iterate through nodearr to label worker nodes
display_step_info "Labeling worker nodes"
for node in "${nodearr[@]}"; do
  kubectl label nodes "${node}" node-role.kubernetes.io/worker=true
done

echo "Set node label to worker nodes."
display_step_info "Adding custom labels to worker nodes"
kubectl label nodes minikube-m02 mcm-type=tx
kubectl label nodes minikube-m03 mcm-type=rx

# Create the custom namespace
display_step_info "Creating the mcm namespace"
kubectl create -f namespace.yaml

# Create PVC
display_step_info "Creating the PVC"
kubectl apply -f pv.yaml
kubectl apply -f pvc.yaml

display_step_info "Deploy Media Proxy DaemonSet"
# kubectl apply -f DaemonSet/media-proxy.yaml
kubectl apply -f DaemonSet/media-proxy-rx.yaml
kubectl apply -f DaemonSet/media-proxy-tx.yaml
