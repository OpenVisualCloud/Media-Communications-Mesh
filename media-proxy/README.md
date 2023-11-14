## MCM Media Proxy

[![build][build-actions-img]][build-actions]
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

### Docker
The Media Proxy can be run as a docker container.
Since Media Proxy depends on the IMTL library, so you need to [setup IMTL](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md) on the host first.

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
The Media Proxy designed to run as DaemonSet of MCM system, to save the system resouce.
There's a sample YAML file integrated in this repo as reference.

```bash
$ kubectl apply -f deployment/DaemonSet/media-proxy.yaml
```

## Known Issues
- There is one bug with default docker.io package installation (version 20.10.25-0ubuntu1~22.04.2) with Ubuntu 22.04.3 LTS. The [`USER` command](https://github.com/moby/moby/issues/46355) and [`chown` command](https://github.com/moby/moby/issues/46161) don't work as expected. It's preferred to install docker-ce package following [instruction from docker community](https://docs.docker.com/engine/install/ubuntu/).

- The Authentication function of Media Proxy interfaces is missing. This feature is under development, and current implentation is weak to defend network attacks.

<!-- References -->
[build-actions-img]: https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/build.yml/badge.svg
[build-actions]: https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/build.yml
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
