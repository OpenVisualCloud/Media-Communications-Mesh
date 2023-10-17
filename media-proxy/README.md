# <img align="center" src="https://avatars.githubusercontent.com/u/17888862?s=200&v=4" alt="drawing" height="48"/> MCM Media Proxy

[![build][build-actions-img]][build-actions]
[![linter][scan-actions-img]][scan-actions]
[![BSD 3-Clause][license-img]][license]

## Table of Contents
- [Usage](#usage)<br>
- [Example](#example)<br>
- [Known Issues](#known-issues)<br>
- [Support](#support)<br>

## Usage
The primary function of the Media Proxy is to provide a single memory-mapped API to all media microservices to abstract away the complexity of media transport.

### Compile
```bash
$ ./build.sh
```

### Run
```bash
$ media_proxy
```

### K8s DaemonSet
The MCM Media Proxy can be run as Kubernetes DaemonSet with following command.
```bash
$ kubectl apply -f deployment/DaemonSet/media-proxy.yaml
```

## Known Issues
- There is one bug with default docker.io package installation (version 20.10.25-0ubuntu1~22.04.2) with Ubuntu 22.04.3 LTS. The [`USER` command](https://github.com/moby/moby/issues/46355) and [`chown` command](https://github.com/moby/moby/issues/46161) don't work as expected. It's preferred to install docker-ce package following [instruction from docker community](https://docs.docker.com/engine/install/ubuntu/).

## Support

<!-- References -->
[build-actions-img]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-proxy/actions/workflows/build.yml/badge.svg
[build-actions]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-proxy/actions/workflows/build.yml
[scan-actions-img]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-proxy/actions/workflows/action.yml/badge.svg
[scan-actions]: https://github.com/intel-innersource/frameworks.cloud.mcm.media-proxy/actions/workflows/action.yml
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
