#!/bin/bash

# Function to display timestamp and execution time
display_step_info() {
    step_num=$((step_num + 1))
    start_time=$(date +%T)
    echo -n "$(date): [Step $step_num] $1"
}

display_step_duration() {
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    printf ' %40s %s\n' " " "Duration: $duration seconds"
}

start_time=$(date +%s)

# Default number of nodes
num_nodes=3
step_num=0

while getopts "n:h" opt; do
    case ${opt} in
        n)
            display_step_info "Setting number of nodes to $OPTARG"
            num_nodes=$OPTARG
            display_step_duration
            ;;
        h)
            echo "Usage: $0 [-n NUM_NODES]" >&2
            exit 0
            ;;
    esac
done

display_step_info "Starting the Minikube cluster with $num_nodes nodes"
minikube start --nodes $num_nodes --namespace mcm
display_step_duration

# Enable minikube addons
display_step_info "Enabling metrics-server addon"
minikube addons enable metrics-server
display_step_duration

# Load docker images into cluster
display_step_info "Loading docker images into the cluster"
minikube image load mcm/media-proxy-dev:latest
minikube image load mcm/sample-app-dev:latest
display_step_duration

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
display_step_duration

echo "Set node label to worker nodes."
display_step_info "Adding custom labels to worker nodes"
kubectl label nodes minikube-m02 mcm-type=tx
kubectl label nodes minikube-m03 mcm-type=rx
display_step_duration

# Create the custom namespace
display_step_info "Creating the mcm namespace"
kubectl create -f namespace.yaml
display_step_duration

# display_step_info "Deploy Media Proxy DaemonSet."
# kubectl apply -f DaemonSet/media-proxy.yaml
# kubectl apply -f DaemonSet/media-proxy-rx.yaml
# kubectl apply -f DaemonSet/media-proxy-tx.yaml

# Calculate and display the total execution time
end_time=$(date +%s)
execution_time=$((end_time - start_time))
echo "Total execution time: $execution_time seconds."
