# Run in Kubernetes — Media Communications Mesh

> ***This document is Work-in-Progress***

The Media Proxy is designed to operate as a DaemonSet within the Media Communications Mesh system
aiming to save system resources. For details, a sample YAML file has been integrated into this repository as a reference.


### Install dependencies

Before deploying the Media Proxy on Kubernetes using Minikube, you will need to ensure that Docker and Minikube are installed.
Follow these steps to install all dependencies:

1. **Install Docker** – You can find installation instructions for Docker based on your operating system in the [Docker's official documentation](https://docs.docker.com/get-docker/).

2. **Install Hypervisor** – Minikube requires a virtualization solution to create a virtual machine. You can use [VirtualBox](https://www.virtualbox.org/) or [KVM](https://www.linux-kvm.org/page/Main_Page) as a hypervisor.

3. **Install kubectl** – Command line tool for interacting with the Kubernetes API server. You can find installation instructions for kubectl [here](https://kubernetes.io/docs/tasks/tools/install-kubectl/).

4. **Install Minikube** – Follow the installation instructions for Minikube based on your operating system in the [Minikube's official documentation](https://minikube.sigs.k8s.io/docs/start/).

Once the above steps are completed, Docker, the hypervisor, kubectl, and Minikube are installed and ready to deploy the Media Proxy on your local Kubernetes cluster.

### Setup Kubernetes cluster
Before deploying Media Proxy to a Kubernetes cluster, execute the following steps to setup the cluster for Media Communications Mesh.

1. Start the Kubernetes cluster and add a worker node to it

   ```bash
   minikube start
   minikube node add -n 1
   ```

2. Assign a label to the worker node.

   ```bash
   kubectl label nodes minikube-m02 mcm.intel.com/role=worker
   ```

### Deploy Media Proxy

```bash
cd Media-Communications-Mesh
kubectl apply -f deployment/DaemonSet/media-proxy.yaml
kubectl get daemonsets.apps -n mcm
```

If case of success, Media Proxy is deployed as a Kubernetes DaemonSet to the worker node
labeled as `"mcm.intel.com/role=worker"`.

## Docker for Mesh Agent

1. Build the Docker image

   ```bash
   cd Media-Communications-Mesh/media-proxy
   docker build --target mesh-agent -t mcm/mesh-agent .
   ```

2. Run the Docker container

   ```bash
   docker run --privileged -p 8100:8100 -p 50051:50051 mcm/mesh-agent:latest
   ```

## Known Issues
- There is a bug with the default docker.io package installation (version 20.10.25-0ubuntu1~22.04.2) with Ubuntu 22.04.3 LTS. The [`USER` command](https://github.com/moby/moby/issues/46355) and [`chown` command](https://github.com/moby/moby/issues/46161) do not work as expected. It is preferred to install the `docker-ce` package, following an [instruction from Docker Docs](https://docs.docker.com/engine/install/ubuntu/).

- The Authentication function of the Media Proxy interfaces is currently missing. This feature is still under development, and the current implementation is weak in defending against network attacks.

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
