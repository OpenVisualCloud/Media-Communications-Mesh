#!/bin/bash

echo "Create MCM cluster."
minikube start --nodes 3 --namespace mcm --insecure-registry "10.0.0.0/24"

echo "Enable minikube addons."
minikube addons enable registry
minikube addons enable metrics-server

echo "Set name for worker nodes."
worker_node_0="minikube-m02"
worker_node_1="minikube-m03"

echo "Load MCM docker image into cluster."
minikube image load mcm/media-proxy:latest

echo "Set node label to MCM worker nodes."
kubectl label nodes ${worker_node_0} node-role.mcm.intel.com/worker=true
kubectl label nodes ${worker_node_1} node-role.mcm.intel.com/worker=true

echo "Deploy Media Proxy DaemonSet."
kubectl create -f DaemonSet/media-proxy.yaml
