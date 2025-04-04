# Media Proxy — Media Communications Mesh

Media Proxy is a core data plane transmission handling component running on every node in the Mesh.
SDK API establishes connections with Media Proxy to send and receive media streams.
   
**Supported streaming options**

* SMPTE ST 2110-20 Uncompressed Video
* SMPTE ST 2110-22 Compressed Video (JPEG XS)
* SMPTE ST 2110-30 Audio
* RDMA, used for inter-node communication

To build Media Proxy, follow the [Setup Guide](SetupGuide.md).

Executable file name: `media_proxy`

Media Proxy should be running in privileged mode.

## Command line arguments

| Arguments           | Description                                                                     | Example                  |
|---------------------|---------------------------------------------------------------------------------|--------------------------|
| `-t` `--sdk`        | Local SDK API listening port number, default 8002                               | `-t 8002`                |
| `-a` `--agent`      | Mesh Agent Proxy API address in the format `host:port`, default localhost:50051 | `-a 192.168.96.1:50051`  |
| `-d` `--st2110_dev` | PCI device port for SMPTE ST 2110 media data streaming, default 0000:31:00.0    | `-d 0000:31:00.0`        |
| `-i` `--st2110_ip`  | IP address for SMPTE ST 2110 connections, default 192.168.96.1                  | `-i 192.168.96.10`       |
| `-r` `--rdma_ip`    | IP address for RDMA connections, default 192.168.96.2                           | `-r 192.168.97.10`       |
| `-p` `--rdma_ports` | Local port ranges for incoming RDMA connections, default 9100-9999              | `-p 9100-9199,8500-8599` |
| `-h` `--help`       | Print usage help                                                                | –                        |

## Environment variables

| Name                     | Description                                                                    | Example                            |
|--------------------------|--------------------------------------------------------------------------------|------------------------------------|
| `MEDIA_PROXY_LCORES`     | List/range of lcores that will be available to Media Proxy.                    | `MEDIA_PROXY_LCORES="1,5-9,64-69"` |
| `MEDIA_PROXY_MAIN_LCORE` | Specify the lcore number to be used for handling the MTL/DPDK main stack/loop. | `MEDIA_PROXY_MAIN_LCORE="32"`      |
| `KAHAWAI_CFG_PATH`       | MTL configuration file path. Refer to the [MTL](https://github.com/OpenVisualCloud/Media-Transport-Library/) documentation. | `KAHAWAI_CFG_PATH="/usr/local/etc/imtl.json"` |

## Run standalone
Example command to run Media Proxy in the host OS

```bash
sudo media_proxy              \
     -t 8002                  \
     -a 192.168.96.1:50051    \
     -d 0000:1f:01.0          \
     -i 192.168.96.10         \
     -r 192.168.97.10         \
     -p 9100-9199
```
Console output
```
Apr 03 10:42:55.084 [INFO] Media Proxy started, version 25.03
Apr 03 10:42:55.084 [DEBU] Set MTL configure file path to /usr/local/etc/imtl.json
Apr 03 10:42:55.084 [INFO] SDK API port: 8002
Apr 03 10:42:55.084 [INFO] MCM Agent Proxy API addr: 192.168.96.1:50051
Apr 03 10:42:55.084 [INFO] ST2110 device port BDF: 0000:1f:01.0
Apr 03 10:42:55.084 [INFO] ST2110 dataplane local IP addr: 192.168.96.10
Apr 03 10:42:55.084 [INFO] RDMA dataplane local IP addr: 192.168.97.10
Apr 03 10:42:55.084 [INFO] RDMA dataplane local port ranges: 9100-9199
Apr 03 10:42:55.094 [INFO] SDK API Server listening on 0.0.0.0:8002
Apr 03 10:42:55.100 [INFO] Media Proxy registered proxy_id="0c456d5a-56c9-443f-970e-2c165c33bcfb"
Apr 03 10:42:55.102 [INFO] [AGENT] ApplyConfig groups=0 bridges=0
Apr 03 10:42:55.104 [INFO] [RECONCILE] Config is up to date
. . .
```

## Run using `native_af_xdp`

To use Media Proxy with the native `af_xdp/ebpf` device, the device name should be
provided with the `native_af_xdp:` prefix, e.g. `media-proxy --dev native_af_xdp:ens259f0np0`.

Notice that the device must have a pre-assigned IP address.
The `-i` parameter is not applied in this mode.

**MtlManager** from the Media Transport Library `manager` subdirectory must be running.
Only a device physical function with a pre-configured IP address can be used for the `native_af_xdp` mode.

## Run in Docker
The Media Proxy can be run in a Docker container.
Since Media Proxy depends on the MTL library, it is required to
[setup MTL](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md) on the host beforehand.

1. Build the Docker image

```bash
cd Media-Communications-Mesh/media-proxy
docker build --target media-proxy -t mcm/media-proxy .
```

2. Run the Docker container

```bash
docker run --privileged -v /dev/vfio:/dev/vfio mcm/media-proxy:latest
```

The `--privileged` argument is necessary to access NIC hardware with the DPDK driver.

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
