# Setting Up Media Communications Mesh Cluster with K8s

## 1. Prerequisites
To set up your Minikube cluster and deploy resources, please ensure that the following prerequisites are met on your local machine:

- **Operating System**: [Ubuntu 22.04](https://releases.ubuntu.com/22.04/)
- **Minikube**: Install Minikube using a package manager or by downloading the binary from the [official website](https://minikube.sigs.k8s.io/docs/start/). Ensure you are using a compatible version with Ubuntu 22.04.
- **kubectl**: This command-line tool allows you to interact with the Kubernetes cluster. You can configure it to connect to your Minikube cluster by running `kubectl config use-context minikube`. Verify that you have a version compatible with your Minikube version.
- **Docker**: Ensure that Docker is installed on your machine. You can install Docker from the [official website](https://docs.docker.com/get-docker/).
- **Images**: Media Communications Mesh cluster running depends on server docker images. Please follow below steps to create them.

### 1.1. Scripted build

To build Dockerfiles Media Proxy, FFmpeg and SDK follow the [guide](./README.md#build-the-docker-images). Sample applications will be available inside `mcm/SDK:latest` Docker image at `/opt/mcm` path. For more information refer to [sample applications](../sdk/README.md#sample-applications). For more advanced and/or production environment usage, we encourage use the FFmpeg based workflow, either minimalistic [Media Communications Mesh FFmpeg plugin](../ffmpeg-plugin/README.md) version or full-capabilities all-in-one [Intel® Tiber™ Broadcast Suite](https://github.com/OpenVisualCloud/Intel-Tiber-Broadcast-Suite).

```bash
# below script accept all docker build parameters, for example fresh rebuild:
# ./build_docker.sh --no-cache
# This depending on your Docker installation type may require using sudo:
# sudo ./build_docker.sh
./build_docker.sh
```

### 1.2. Manual build

Manual build should be done with build context pointing to the root directory of Media Communications Mesh repository, you should prefer using `./build_docker.sh` instead, as the script guarantee a path independent execution of build process.

Example of manual build:

```bash
cd <mcm-dir>
docker build --build-arg=http_proxy --build-arg=https_proxy --build-arg=no_proxy \
      -t mcm/media-proxy:custom-build" \
      -f <mcm-dir>/media-proxy/Dockerfile \
      "<mcm-dir>"
```

### 1.3. MTL Manager build:

To build MTL Manager you need to fetch and build MTL Library from the source, this can be done by following below commands:

```bash
git clone https://github.com/OpenVisualCloud/Media-Transport-Library.git <mtl-dir>
cd <mtl-dir>/mananger
docker build -t mtl-manager:latest .
```

- PS: Detail information about the MTL Manager could refer to the [MTL document](https://github.com/OpenVisualCloud/Media-Transport-Library/tree/main/manager).

Once these prerequisites are in place, you can proceed with setting up your Minikube cluster and deploying resources. If you encounter any issues during the process, feel free to ask for assistance.

## 2. Setup Steps

### 2.1. Launch MTL Manager on the host server.
The MTL Manager is needed to manage the lcore for MTL instances. It needs to be run on each physical host server of the Media Communications Mesh cluster.
```bash
docker run -d \
  --name mtl-manager \
  --privileged --net=host \
  -v /var/run/imtl:/var/run/imtl \
  mtl-manager:latest
```

### 2.2. Setting Up the Minikube Cluster
Use the following command to set the number of nodes for your Minikube cluster:
```bash
minikube start --nodes $num_nodes --namespace mcm --mount-string="/var/run/imtl:/var/run/imtl" --mount
```

### 2.3. Enabling add-ons and Loading Docker Images
Once the Minikube cluster is running, enable the metrics-server add-on:
```bash
minikube addons enable metrics-server
```

- Load the required Docker images into the Minikube cluster:
```bash
minikube image load mcm/media-proxy:latest
minikube image load mcm/sample-app:latest
```


### 2.4. Labeling Worker Nodes
After the cluster is set up, label the worker nodes with the following commands:
```bash
kubectl label nodes minikube-m02 mcm-type=tx
kubectl label nodes minikube-m03 mcm-type=rx
```

### 2.5. Creating Resources
Apply the custom namespace and create the Persistent Volume Claim (PVC) using the provided YAML files:
```bash
kubectl create -f namespace.yaml
kubectl apply -f pv.yaml
kubectl apply -f pvc.yaml
```

### 2.6. Deploying DaemonSets
Deploy the Media Proxy DaemonSets for both receiver and transmitter using the provided YAML files:
```bash
kubectl apply -f DaemonSet/media-proxy-rx.yaml
kubectl apply -f DaemonSet/media-proxy-tx.yaml
```

## 3. Summary
This user guide outlined the steps to set up a Media Communications Mesh cluster, enable addons, load Docker images, label worker nodes, create resources, and deploy Media Proxy. By following these steps, you can configure a Minikube cluster for your Media Communications Mesh system.

**Note:** Users can simply run the `deployment/setup.sh` script to execute all the mentioned steps in this user guide.
