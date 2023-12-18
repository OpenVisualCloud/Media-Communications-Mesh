# Setting Up MCM Cluster with K8s

## Prerequisites
To set up your Minikube cluster and deploy resources, please ensure that the following prerequisites are met on your local machine:

- **Operating System**: [Ubuntu 22.04](https://releases.ubuntu.com/22.04/)
- **Minikube**: Install Minikube using a package manager or by downloading the binary from the [official website](https://minikube.sigs.k8s.io/docs/start/). Ensure you are using a compatible version with Ubuntu 22.04.
- **kubectl**: This command-line tool allows you to interact with the Kubernetes cluster. You can configure it to connect to your Minikube cluster by running `kubectl config use-context minikube`. Verify that you have a version compatible with your Minikube version.
- **Docker**: Ensure that Docker is installed on your machine. You can install Docker from the [official website](https://docs.docker.com/get-docker/).
- **Images**: MCM cluster running depends on server docker images. Please follow below steps to create them.

Media Proxy:
```bash
cd <mcm>/media-proxy
docker build -t mcm/media-proxy:latest .
```

Sample Applications:
```bash
cd <mcm>/sdk
docker build -t mcm/sample-app:latest .
```

IMTL Manager:
```bash
git clone https://github.com/OpenVisualCloud/Media-Transport-Library.git <imtl-dir>
cd <imtl-dir>/mananger
docker build -t mtl-manager:latest .
```

- PS: Detail information about the IMTL Manager could refer to the [IMTL document](https://github.com/OpenVisualCloud/Media-Transport-Library/tree/main/manager).

Once these prerequisites are in place, you can proceed with setting up your Minikube cluster and deploying resources. If you encounter any issues during the process, feel free to ask for assistance.

## Setup Steps

### 1. Launch IMTL Manager on the host server.
The IMTL Manager is needed to manage the lcore for MTL instances. It needs to be run on each physical host server of the MCM cluster.
```bash
docker run -d \
  --name mtl-manager \
  --privileged --net=host \
  -v /var/run/imtl:/var/run/imtl \
  mtl-manager:latest
```

### 2. Setting Up the Minikube Cluster
Use the following command to set the number of nodes for your Minikube cluster:
```bash
minikube start --nodes $num_nodes --namespace mcm --mount-string="/var/run/imtl:/var/run/imtl" --mount
```

### 3. Enabling Addons and Loading Docker Images
Once the Minikube cluster is running, enable the metrics-server addon:
```bash
minikube addons enable metrics-server
```

- Load the required Docker images into the Minikube cluster:
```bash
minikube image load mcm/media-proxy:latest
minikube image load mcm/sample-app:latest
```


### 4. Labeling Worker Nodes
After the cluster is set up, label the worker nodes with the following commands:
```bash
kubectl label nodes minikube-m02 mcm-type=tx
kubectl label nodes minikube-m03 mcm-type=rx
```

### 5. Creating Resources
Apply the custom namespace and create the Persistent Volume Claim (PVC) using the provided YAML files:
```bash
kubectl create -f namespace.yaml
kubectl apply -f pv.yaml
kubectl apply -f pvc.yaml
```

### 6. Deploying DaemonSets
Deploy the Media Proxy DaemonSets for both receiver and transmitter using the provided YAML files:
```bash
kubectl apply -f DaemonSet/media-proxy-rx.yaml
kubectl apply -f DaemonSet/media-proxy-tx.yaml
```

## Summary
This user guide outlined the steps to set up a MCM cluster, enable addons, load Docker images, label worker nodes, create resources, and deploy Media Proxy. By following these steps, you can configure a Minikube cluster for your MCM system.

**Note:** Users can simply run the `deployment/setup.sh` script to execute all the mentioned steps in this user guide.
