# <img align="center" src="https://avatars.githubusercontent.com/u/17888862?s=200&v=4" alt="drawing" height="48"/> Media Communications Mesh

[![build][build-actions-img]][build-actions]
[![linter][scan-actions-img]][scan-actions]
[![BSD 3-Clause][license-img]][license]

## Table of Contents
- [Overview](#overview)<br>
- [Usage](#usage)<br>
- [Example](#example)<br>
- [Known Issues](#known-issues)<br>
- [Support](#support)<br>

## Overview
The Media Communications Mesh (MCM) enables efficient, low-latency media transport for media microservices for Edge, Edge-to-Cloud, and both private and public Cloud environments. The framework creates a secure, standards-based media
data plane for inter-microservices communications using a new media proxy leveraging the [Intel® Media Transport Library (IMTL)](https://github.com/OpenVisualCloud/Media-Transport-Library) and adds the necessary microservices control-plane communications infrastructure to implement any media control protocol.

## Usage

### Build & Install
```bash
$ ./build.sh
```

## Example
```bash
$ cd build/out
$ ./sender_app
$ ./recver_app
```

## Known Issues
- There is one bug with default docker.io package installation (version 20.10.25-0ubuntu1~22.04.2) with Ubuntu 22.04.3 LTS. The [`USER` command](https://github.com/moby/moby/issues/46355) and [`chown` command](https://github.com/moby/moby/issues/46161) don't work as expected. It's preferred to install docker-ce package following [instruction from docker community](https://docs.docker.com/engine/install/ubuntu/).

## Support

<!-- References -->
[build-actions-img]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-communications-mesh/actions/workflows/build.yml/badge.svg
[build-actions]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-communications-mesh/actions/workflows/build.yml
[scan-actions-img]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-communications-mesh/actions/workflows/action.yml/badge.svg
[scan-actions]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-communications-mesh/actions/workflows/action.yml
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
