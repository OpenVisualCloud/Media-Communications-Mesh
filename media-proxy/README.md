# Media Proxy

![Media Proxy](../docs/_static/media-proxy-media-communications-mesh-1.png)

[![Ubuntu Build](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml/badge.svg)](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml)
[![BSD 3-Clause][license-img]][license]

The primary function of the Media Proxy is to provide a single memory-mapped API to all media microservices to abstract away the complexity of media transport.

## Compile and Install

```bash
./build.sh
```

## Run
Media Proxy can be started with the following command

```bash
media_proxy
```
```text
Dec 31 11:03:00.377 [INFO] Media Proxy started
Dec 31 11:03:00.377 [DEBU] Set MTL configure file path to /usr/local/etc/imtl.json
Dec 31 11:03:00.377 [INFO] SDK API port: 8002
Dec 31 11:03:00.377 [INFO] MCM Agent Proxy API addr: localhost:50051
Dec 31 11:03:00.377 [INFO] ST2110 device port BDF: 0000:31:00.0
Dec 31 11:03:00.377 [INFO] ST2110 dataplane local IP addr: 192.168.96.1
Dec 31 11:03:00.377 [INFO] RDMA dataplane local IP addr: 192.168.96.2
Dec 31 11:03:00.377 [INFO] RDMA dataplane local port ranges: 9100-9999
Dec 31 11:03:00.392 [INFO] TCP Server listening on 0.0.0.0:18002
Dec 31 11:03:00.393 [INFO] SDK API Server listening on 0.0.0.0:8002
Dec 31 11:03:00.394 [INFO] [AGENT] ApplyConfig groups=0 bridges=0
Dec 31 11:03:00.394 [INFO] [RECONCILE] Config is up to date
```

The Media Proxy requires Mesh Agent to be running and listening at `localhost:50051` for incoming gRPC requests.

If Media Proxy successfully launches up, it opens a port for listening to control requests from SDK via gRPC (default 8002).

All supported parameters for `media_proxy` can be printed by passing `-h` argument as follows:

```bash
media_proxy -h
```
```text
Usage: media_proxy [OPTION]
-h, --help                      Print this help and exit
-t, --sdk=port_number           Port number for SDK API server (default: 8002)
-a, --agent=host:port           MCM Agent Proxy API address in the format host:port (default: localhost:50051)
-d, --st2110_dev=dev_port       PCI device port for SMPTE 2110 media data transportation (default: 0000:31:00.0)
-i, --st2110_ip=ip_address      IP address for SMPTE 2110 (default: 192.168.96.1)
-r, --rdma_ip=ip_address        IP address for RDMA (default: 192.168.96.2)
-p, --rdma_ports=ports_ranges   Local port ranges for incoming RDMA connections (default: 9100-9999)
```

Additionally, by having environment variables `MEDIA_PROXY_MAIN_LCORE`, `MEDIA_PROXY_LCORES` or `KAHAWAI_CFG_PATH` defined in your `shell` environment before the execution of `media_proxy` you can influence more low level behaviour as explained bellow. Setting the variables is optional and should be accompanied by OS pre-configuration:

```
MEDIA_PROXY_LCORES="1,5-9,64-69"..: list/range of lcores that will available to media_proxy stack for usage.
MEDIA_PROXY_MAIN_LCORE="32".......: force lcore number passed to be used for handling DPDK/MTL main stack/loop.
KAHAWAI_CFG_PATH="/etc/mtl.json"..: absolute path to the exact location of mtl.json file [refer MTL for help].
```

### Run Media Proxy using `native_af_xdp`

To use Media Proxy with the native `af_xdp/ebpf` device, a device name should be provided with the `native_af_xdp:` prefix, for example `media-proxy --dev native_af_xdp:ens259f0np0`.
Notice that the device must have a pre-assigned IP address. The `--ip` parameter is not applied in this mode.

> [!CAUTION]
> `MtlManager` from the Media-Transport-Library `manager` subdirectory must be running.
> Only a device physical function with a pre-configured IP address can be used for the `native_af_xdp` mode.

## Docker
The Media Proxy can be run as a docker container.
Since Media Proxy depends on the MTL library, so you need to [setup MTL](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md) on the host first.

1. Build Docker Image

```bash
cd Media-Communications-Mesh/media-proxy
docker build --target media-proxy -t mcm/media-proxy .
```

2. Run the Docker Container

```bash
docker run --privileged -v /dev/vfio:/dev/vfio mcm/media-proxy:latest
```

The "--privileged" argument is necessary to access NIC hardware with DPDK driver.

## Kubernetes
The Media Proxy is designed to operate as a DaemonSet within the Media Communications Mesh system, aiming to conserve system resources. For details, a sample YAML file has been integrated into this repository as a reference.

To deploy the Media Proxy as a DaemonSet in your Kubernetes cluster, you can use the following command:

## Dependencies Installation

Before deploying the Media Proxy on Kubernetes using Minikube, you'll need to ensure that Docker & Minikube are installed. Follow these steps to install all dependencies:

1. **Install Docker**: You can find installation instructions for Docker based on your operating system from [Docker's official documentation](https://docs.docker.com/get-docker/).

2. **Install a Hypervisor**: Minikube requires a virtualization solution to create a virtual machine. You can use [VirtualBox](https://www.virtualbox.org/) or [KVM](https://www.linux-kvm.org/page/Main_Page) as the hypervisor.

3. **Install kubectl**: kubectl is a command line tool for interacting with the Kubernetes API server. You can find installation instructions for kubectl [here](https://kubernetes.io/docs/tasks/tools/install-kubectl/).

4. **Install Minikube**: Follow the installation instructions for Minikube based on your operating system from [Minikube's official documentation](https://minikube.sigs.k8s.io/docs/start/).

Once you've completed the above steps, you'll have Docker, a hypervisor, kubectl, and Minikube installed and ready to deploy the Media Proxy on your local Kubernetes cluster.

### Setup K8s Cluster
Before deploying Media Proxy to the K8s cluster, you need to execute the following steps to setup the K8s cluster to be ready for Media Communications Mesh.

1. Start the K8s Cluster, and add worker node on it.

```bash
minikube start
minikube node add -n 1
```

2. Set the label for the worker node.

```bash
kubectl label nodes minikube-m02 mcm.intel.com/role=worker
```

### Deploy Media Proxy

```bash
cd Media-Communications-Mesh
kubectl apply -f deployment/DaemonSet/media-proxy.yaml
kubectl get daemonsets.apps -n mcm
```

If all commands are executed successfully, you will see the Media Proxy deployed as a K8s DaemonSet to the worker node (labeled with "mcm.intel.com/role=worker").

## Docker for mcm/mesh-agent

1. Build Docker Image

```bash
cd Media-Communications-Mesh/media-proxy
docker build --target mesh-agent -t mcm/mesh-agent .
```

2. Run the Docker Container

```bash
docker run --privileged -p 8100:8100 -p 50051:50051 mcm/mesh-agent:latest
```

## Known Issues
- There is a bug with the default docker.io package installation (version 20.10.25-0ubuntu1~22.04.2) with Ubuntu 22.04.3 LTS. The [`USER` command](https://github.com/moby/moby/issues/46355) and [`chown` command](https://github.com/moby/moby/issues/46161) do not work as expected. It is preferred to install the `docker-ce` package, following an [instruction from Docker Docs](https://docs.docker.com/engine/install/ubuntu/).

- The Authentication function of the Media Proxy interfaces is currently missing. This feature is still under development, and the current implementation is weak in defending against network attacks.

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
