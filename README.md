# <img align="center" src="docs/img/logo.png" alt="drawing" height="50"/> Media Communications Mesh

[![Ubuntu Build](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml/badge.svg)](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml)
[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/OpenVisualCloud/Media-Communications-Mesh/badge)](https://securityscorecards.dev/viewer/?uri=github.com/OpenVisualCloud/Media-Communications-Mesh)
[![BSD 3-Clause][license-img]][license]

## Table of Contents
- [Introduction](#introduction)
- [Media Proxy](#media-proxy)
- [MCM SDK](#mcm-sdk)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Known Issues](#known-issues)
- [Support](#support)

## Introduction

The Media Communications Mesh (MCM) enables efficient, low-latency media transport for media microservices for Edge, Edge-to-Cloud, and both private and public Cloud environments. The framework creates a secure, standards-based media data plane for inter-microservices communications using a new media proxy leveraging the [Intel® Media Transport Library (IMTL)](https://github.com/OpenVisualCloud/Media-Transport-Library) and adds the necessary microservices control-plane communications infrastructure to implement any media control protocol.

## Media Proxy

The Media Proxy component in this project is responsible for transporting audio and video stream media data. It handles the routing and forwarding of media data between mesh nodes, ensuring low latency and efficient resource utilization.

It serves as a Data Plane component in a media application's Service Mesh. Media Proxy provides Shared Memory APIs to abstract the complexity of media transport.

More information about the Media Proxy component can be found in the [media-proxy](media-proxy) subdirectory.

### Key Features

- Enables zero memory copy and ultra-low-latency media transfers between containers.
- Supports transport of video streams via compressed (JPEG XS) or RAW (uncompressed) protocols.
- Offers compatibility with multiple media transport protocols (SMPTE ST 2110, RTSP, ...).

### MemIF

The Media Proxy leverages the [MemIF](https://s3-docs.fd.io/vpp/24.02/interfacing/libmemif/index.html) protocol for implementing shared memory buffer communications.

## MCM SDK

The MCM SDK library is designed for microservice applications to offload media transport functionalities. By abstracting network transport functions for media data and wrapping the "libmemif" APIs, MCM SDK facilitates communication with "Media Proxy".

Detailed information about MCM SDK can be found in [sdk](sdk) directory.

## Getting Started

### Prerequisites
- Linux server (Intel Xeon processor recommended)
- Network Interface Card (NIC) compatible with DPDK
- Docker installed

### Basic Installation

0. **Install Dependencies**
    - IMTL: Follow the [IMTL setup guide](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/build.md) for installation.
    - gRPC: Refer to the [gRPC documentation](https://grpc.io/docs/languages/cpp/quickstart/) for installation instructions.

1. **Clone the repository**
   ```sh
   $ git clone https://github.com/OpenVisualCloud/Media-Communications-Mesh.git
   ```

2. Navigate to the Media Proxy directory
    ```sh
    $ cd Media-Communications-Mesh
    ```

3. Build the Media Proxy binary (run on Host)
    ```sh
    $ ./build.sh
    ```

4. Build the Media Proxy Docker image (run in Container)
    ```sh
    $ docker build -t media-proxy ./media-proxy
    ```

By following these instructions, you'll be able to perform the basic installation of the Media Communications Mesh application.

## Usage

The program "media_proxy" and SDK library can be installed on system, after the "build.sh" script run successfully.
And the "Media Proxy" can be run with below command.

```bash
$ media_proxy
INFO: TCP Server listening on 0.0.0.0:8002
INFO: gRPC Server listening on 0.0.0.0:8001
```

This will start the "Media Proxy" program. When the "Media Proxy" program launches successfully, it will open two ports to listen for control messages:
- gRPC port (default 8001) is for service mesh control plane connection.
- TCP port (default 8002) is for the connection with MCM SDK.

To get a list of all supported parameters, use the "helper" function with the following command:

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
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
