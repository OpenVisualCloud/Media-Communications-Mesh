# Media Communications Mesh Data Plane SDK

[![Ubuntu Build](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml/badge.svg)](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml)
[![BSD 3-Clause][license-img]][license]

## Overview

The Media Communications Mesh SDK (further referred to as SDK) is a lightweight and versatile library designed to facilitate seamless media data transportation between microservices, with or without the Media Proxy. By leveraging the SDK, developers can easily establish efficient media streaming and enable zero-copied handling of media data transfers.

## Installation

1. Dependencies

Install the required dependencies by running the command:

```bash
sudo apt-get install -y cmake libbsd-dev
```

2. Build SDK Library

```bash
cmake -B out .
cmake --build out -j 4
```

3. Install on System

To install the SDK on your system, execute the following command:

```bash
cmake --install out
```

## Sample Applications

The usage of SDK APIs could refer to the "samples" applications.

1. Sender

Sample code for the application which send out data to others.

Source code: samples/sender_app.c
```bash
./build/samples/sender_app
```
```text
usage: sender_app [OPTION]
-h, --help                      Print this help and exit.
-s, --ip=ip_address             Send data to IP address (default: 127.0.0.1).
-p, --port=port_number          Send data to Port (default: 9001).
-n, --number=frame_number       Total frame number to send (default: 300).
```

2. Receiver

Sample code for the application which receive data from others.

Source code: samples/sender_app.c
```bash
./build/samples/recver_app
```
```text
usage: recver_app [OPTION]
-h, --help                      Print this help and exit.
-r, --ip=ip_address             Receive data from IP address (defaults: 127.0.0.1).
-p, --port=port_number  Receive data from Port (defaults: 9001).
```

3. RAISR Microservice

Standalone microservice to apply "super resolution" to RAW YUV format video frames.
This application use Media Communications Mesh DP SDK to handle on the data input/output functions.

Source code: (will be released later)

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
