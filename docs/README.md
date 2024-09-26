# <img align="center" src="img/logo.png" alt="drawing" height="50"/> Media Communications Mesh

[![Ubuntu Build](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml/badge.svg)](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml)
[![Coverity status](https://scan.coverity.com/projects/30272/badge.svg?flat=1)](https://scan.coverity.com/projects/media-communications-mesh)
[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/OpenVisualCloud/Media-Communications-Mesh/badge)](https://securityscorecards.dev/viewer/?uri=github.com/OpenVisualCloud/Media-Communications-Mesh)
[![BSD 3-Clause][license-img]][license]

## Table of Contents
- [ Media Communications Mesh](#-media-communications-mesh)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Media Proxy](#media-proxy)
    - [Key Features](#key-features)
    - [MemIF](#memif)
  - [MCM SDK](#mcm-sdk)
  - [Getting Started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Basic Installation](#basic-installation)
  - [Usage](#usage)
  - [Known Issues](#known-issues)
  - [Support](#support)
  - [Note](#note)

## Introduction

The Media Communications Mesh (MCM) enables efficient, low-latency media transport for media microservices for Edge, Edge-to-Cloud, and both private and public Cloud environments. The framework creates a secure, standards-based media data plane for inter-microservices communications using a new media proxy leveraging the [Media Transport Library (MTL)](https://github.com/OpenVisualCloud/Media-Transport-Library) and adds the necessary microservices control-plane communications infrastructure to implement any media control protocol.

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

1. **Install Dependencies**, choose between options `a)` or `b)`.

    a) The `build_dep.sh` script can be used for all-in-one environment preparation. The script is designed for Debian and was tested under `Ubuntu 20.04`, `Ubuntu 22.04` and `Ubuntu 24.04` environments.
   To use this option just run the script after cloning the repository by typing `build_dep.sh`

    b) The following method is universal and should work for most Linux OS distributions.

    - MTL: Follow the [MTL setup guide](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/build.md) for installation.
    - gRPC: Refer to the [gRPC documentation](https://grpc.io/docs/languages/cpp/quickstart/) for installation instructions.
    - Install required packages:
        - Ubuntu/Debian
        ```bash
        sudo apt-get update
        sudo apt-get install libbsd-dev
        ```
        - Centos stream
        ```bash
        sudo yum install -y libbsd-devel
        ```
    - Install the irdma driver and libfabric
    ```bash
    ./scripts/setup_rdma_env.sh install
    ```
    - Reboot

2. **Clone the repository**
   ```sh
   $ git clone https://github.com/OpenVisualCloud/Media-Communications-Mesh.git
   ```

3. **Navigate to the Media-Communications-Mesh directory**
    ```sh
    $ cd Media-Communications-Mesh
    ```

4. **Build the Media Proxy binary (run on Host)**
    ```sh
    $ ./build.sh
    ```

5. **Build the Media Proxy Docker image (run in Container)**
    ```sh
    $ ./build_docker.sh
    ```

By following these instructions, you'll be able to perform the basic build and installation of the Media Communications Mesh application.

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

- The Authentication function of the Media Proxy interfaces is currently missing. This feature is still under development, and the current implementation is weak in defending against network attacks.

## Support

If you encounter any issues or need assistance, there are several ways to seek support:

- **Project Documents:** Search for solutions in the [project documents](https://github.com/OpenVisualCloud/Media-Communications-Mesh/tree/main/docs).
- **Discussions:** Ask questions and seek help in the [Discussions](https://github.com/OpenVisualCloud/Media-Communications-Mesh/discussions/categories/q-a) section on the project's GitHub page.
- **Issue Submission:** Report bugs and specific issues by submitting them on the [Issues](https://github.com/OpenVisualCloud/Media-Communications-Mesh/issues) page of the project's GitHub repository.

Before submitting an issue, please check the existing documentation and discussions to avoid duplication and streamline the support process.

We are here to help, so don't hesitate to reach out if you need assistance.

## Note

This project is under development.
All source code and features on the main branch are for the purpose of testing or evaluation and not production ready.

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
