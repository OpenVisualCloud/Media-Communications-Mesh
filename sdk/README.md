# MCM Data Plane SDK

[![Ubuntu Build](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml/badge.svg)](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml)
[![BSD 3-Clause][license-img]][license]

## Overview
The SDK library designed to make microservice applications transport media data through MCM media-proxy.

## Build
```bash
$ cmake -B build .
$ cmake --build build -j
```

## Sample Applications
```bash
$ cd build/samples
$ ./sender_app
$ ./recver_app
```

## Install
```bash
$ cmake --install build
```

## Sample Applications
The usage of SDK APIs could refer to the "samples" applications.
1. Sender

Sample code for the application which send out data to others.

Source code: samples/sender_app.c
```bash
$ ./build/samples/sender_app
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
$ ./build/samples/recver_app
usage: recver_app [OPTION]
-h, --help                      Print this help and exit.
-r, --ip=ip_address             Receive data from IP address (defaults: 127.0.0.1).
-p, --port=port_number  Receive data from Port (defaults: 9001).
```

3. RAISR Microservice

Standalone microservice to apply "super resolution" to RAW YUV format video frames.
This application use MCM DP SDK to handle on the data input/output functions.

Source code: https://github.com/intel-sandbox/raisr-microservice

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
