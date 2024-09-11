## MCM Media Proxy

[![Ubuntu Build](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml/badge.svg)](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml)
[![BSD 3-Clause][license-img]][license]

The primary function of the Media Proxy is to provide a single memory-mapped API to all media microservices to abstract away the complexity of media transport.

### Compile
```bash
$ ./build.sh
```

### Run
The program "media_proxy" can be installed on system, after the "build.sh" script run successfully.
And the "Media Proxy" can be run with below command.

```bash
$ media_proxy
INFO: TCP Server listening on 0.0.0.0:8002
INFO: gRPC Server listening on 0.0.0.0:8001
```

If Media Proxy successfully launches up, it will open 2 port to listen on control messages.
- gRPC port (default 8001) is for service mesh control plane connection.
- TCP port (default 8002) is for the connection with MCM SDK.

All supported parameters can get with the program "helper" function.
```bash
$ media_proxy -h
Usage: media_proxy [OPTION]
-h, --help              Print this help and exit.
-d, --dev=dev_port      PCI device port (defaults: 0000:31:00.0).
-i, --ip=ip_address     IP address for media data transportation (defaults: 192.168.96.1).
-g, --grpc=port_number  Port number gRPC controller (defaults: 8001).
-t, --tcp=port_number   Port number for TCP socket controller (defaults: 8002).
```

#### Run media-proxy using `native_af_xdp`

To use media-proxy with the native `af_xdp/ebpf` device a device name should be provided with the `native_af_xdp:` prefix, for example `media-proxy --dev native_af_xdp:ens259f0np0`.
Notice that the device must have a pre-assigned IP address. The `--ip` parameter is not applied in this mode.

> [!CAUTION]
> `MtlManager` from the Media-Transport-Library `manager` subdirectory must be running.
> Only a device physical function with a preconfigured IP address can be used for the `native_af_xdp` mode.

### Docker
The Media Proxy can be run as a docker container.
Since Media Proxy depends on the MTL library, so you need to [setup MTL](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md) on the host first.

1. Build Docker Image

```bash
cd Media-Communications-Mesh/media-proxy
docker build -t mcm/media-proxy .
```

2. Run the Docker Container

```bash
docker run --privileged -v /dev/vfio:/dev/vfio mcm/media-proxy:latest
```

The "--privileged" argument is necessary to access NIC hardware with DPDK driver.

### Kubernetes
The Media Proxy is designed to operate as a DaemonSet within the MCM system, aiming to conserve system resources. For details, a sample YAML file has been integrated into this repository as a reference.

To deploy the Media Proxy as a DaemonSet in your Kubernetes cluster, you can use the following command:

### Dependencies Installation

Before deploying the Media Proxy on Kubernetes using Minikube, you'll need to ensure that Docker & Minikube are installed. Follow these steps to install all dependencies:

1. **Install Docker**: You can find installation instructions for Docker based on your operating system from [Docker's official documentation](https://docs.docker.com/get-docker/).

2. **Install a Hypervisor**: Minikube requires a virtualization solution to create a virtual machine. You can use [VirtualBox](https://www.virtualbox.org/) or [KVM](https://www.linux-kvm.org/page/Main_Page) as the hypervisor.

3. **Install kubectl**: kubectl is a command line tool for interacting with the Kubernetes API server. You can find installation instructions for kubectl [here](https://kubernetes.io/docs/tasks/tools/install-kubectl/).

4. **Install Minikube**: Follow the installation instructions for Minikube based on your operating system from [Minikube's official documentation](https://minikube.sigs.k8s.io/docs/start/).

Once you've completed the above steps, you'll have Docker, a hypervisor, kubectl, and Minikube installed and ready to deploy the Media Proxy on your local Kubernetes cluster.

#### Setup K8s Cluster
Before deploy media proxy to the K8s cluster, you need to execute following steps to setup the K8s cluster to be ready for MCM.

1. Start the K8s Cluster, and add MCM worker node on it.

```bash
$ minikube start
$ minikube node add -n 1
```

2. Set the label for the worker node.

```bash
$ kubectl label nodes minikube-m02 mcm.intel.com/role=worker
```

#### Deploy Media Proxy

```bash
$ cd Media-Communications-Mesh
$ kubectl apply -f deployment/DaemonSet/media-proxy.yaml
$ kubectl get daemonsets.apps -n mcm
```

If all commands are executed successfully, you will see the MCM media proxy deployed as a K8s DaemonSet to the MCM worker node (labeled with "mcm.intel.com/role=worker").

## Known Issues
- There is one bug with default docker.io package installation (version 20.10.25-0ubuntu1~22.04.2) with Ubuntu 22.04.3 LTS. The [`USER` command](https://github.com/moby/moby/issues/46355) and [`chown` command](https://github.com/moby/moby/issues/46161) don't work as expected. It's preferred to install docker-ce package following [instruction from docker community](https://docs.docker.com/engine/install/ubuntu/).

- The Authentication function of the Media Proxy interfaces is currently missing. This feature is still under development, and the current implementation is weak in defending against network attacks.

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
