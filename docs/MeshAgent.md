# Mesh Agent — Media Communications Mesh

Mesh Agent is a control plane component responsible for establishing appropriate traffic topology
in the Mesh according to user app requests. It handles the life cycle of connections
and multipoint groups, configures egress and ingress bridges of SMPTE ST 2110 and RDMA types,
and collects Media Proxy metrics.

To build Mesh Agent, follow the [Setup Guide](SetupGuide.md).

Executable file name: `mesh_agent`

Privileged mode is **NOT** required.

## Command line arguments

| Argument | Description                                           | Example    |
|:--------:|-------------------------------------------------------|:----------:|
| `-p`     | Local Proxy API listening port number, default 50051  | `-p 50051` |
| `-c`     | Local Control API listening port number, default 8100 | `-c 8100`  |
| `-v`     | Verbose output                                        | –          |
| `-h`     | Print usage help                                      | –          |

## Example

```bash
mesh-agent -p 50051
```
Console output
```
INFO[0000] Mesh Control Plane Agent started, version 25.03
INFO[0000] Server starts listening at :50051 - Media Proxy API (gRPC) 
INFO[0000] Server starts listening at :8100 - Control Plane API (REST)
. . .
```

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
