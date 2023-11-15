# <img align="center" src="docs/img/logo.png" alt="drawing" height="50"/> Media Communications Mesh

[![build][build-actions-img]][build-actions]
[![linter][scan-actions-img]][scan-actions]
[![BSD 3-Clause][license-img]][license]

## Table of Contents
- [Overview](#overview)
- [Media Proxy](#media-proxy)
- [MCM SDK](#mcm-sdk)
- [Usage](#usage)
- [Known Issues](#known-issues)
- [Support](#support)

## Overview
The Media Communications Mesh (MCM) enables efficient, low-latency media transport for media microservices for Edge, Edge-to-Cloud, and both private and public Cloud environments. The framework creates a secure, standards-based media
data plane for inter-microservices communications using a new media proxy leveraging the [Intel® Media Transport Library (IMTL)](https://github.com/OpenVisualCloud/Media-Transport-Library) and adds the necessary microservices control-plane communications infrastructure to implement any media control protocol.

## Media Proxy
The Media Proxy can work as a Data Plane component in "Service Mesh" for media applications. It provides "Shared Memory" APIs to all media microservices to abstract away the complexity of media transport.

Detailed information about Media Proxy can be found under the [media-proxy](media-proxy) subdirectory.

### Key features:
- Zero memory copy, ultra low-latency media transfers between containers.
- Transport video stream with compressed (JPEG XS) or RAW (uncompressed) protocols.
- Support multiple media transport protocols (SMPTE ST 2110, RTSP, ...).

### MemIF
Media Proxy implements the shared memory buffer communications using [MemIF](https://s3-docs.fd.io/vpp/24.02/interfacing/libmemif/index.html) protocol.

## MCM SDK
The MCM SDK library can be used by microservice application to offload the media transport functionalities.
It is designed to abstract the network transport functions for media data, and wrap the "libmemif" APIs to communicate with "Media Proxy".

Detailed information about MCM SDK can be found in [sdk](sdk) directory.

## Usage

### Build & Install
The MCM source code can be compiled with the "build.sh" script.

```bash
$ ./build.sh
```

### Run
The program "media_proxy" and SDK library can be installed on system, after the "build.sh" script run successfully.
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

## Known Issues
- There is one bug with default docker.io package installation (version 20.10.25-0ubuntu1~22.04.2) with Ubuntu 22.04.3 LTS. The [`USER` command](https://github.com/moby/moby/issues/46355) and [`chown` command](https://github.com/moby/moby/issues/46161) don't work as expected. It's preferred to install docker-ce package following [instruction from docker community](https://docs.docker.com/engine/install/ubuntu/).

- The Authentication function of Media Proxy interfaces is missing. This feature is under development, and current implentation is weak to defend network attacks.

## Support
If you have any problem during use this project, you could find support in following ways.
- Search in the project [documents](https://github.com/OpenVisualCloud/Media-Communications-Mesh/tree/main/docs)
- Ask your question in the [Discussions](https://github.com/OpenVisualCloud/Media-Communications-Mesh/discussions/categories/q-a)
- Submit [Issues](https://github.com/OpenVisualCloud/Media-Communications-Mesh/issues) for bug.

## Note
This project is under development.
All source code and features on the main branch are for the purpose of testing or evaluation and not production ready.

<!-- References -->
[build-actions-img]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-communications-mesh/actions/workflows/build.yml/badge.svg
[build-actions]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-communications-mesh/actions/workflows/build.yml
[scan-actions-img]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-communications-mesh/actions/workflows/action.yml/badge.svg
[scan-actions]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-communications-mesh/actions/workflows/action.yml
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
