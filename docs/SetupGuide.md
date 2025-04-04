# Setup Guide — Media Communications Mesh

## Prerequisites

* Linux server – Intel Xeon processor recommended, e.g. Sapphire Rapids.
* NIC – Network interface card compatible with DPDK, e.g. Intel Ethernet Controller E810-C.
* Updated NIC drivers and firmware – Strong recommendation is to update them. For more information and the latest drivers, see [Support](#4-support).

## Build and install steps

1. Clone the repository

   ```bash
   git clone https://github.com/OpenVisualCloud/Media-Communications-Mesh.git
   ```

1. Navigate to the Media-Communications-Mesh directory

    ```bash
    cd Media-Communications-Mesh
    ```

1. Install dependencies. There are two options.

    * **Option A** – Use environment preparation scripts. The scripts were tested under environments with `Ubuntu 20.04`, `Ubuntu 22.04`, `Ubuntu 24.04`, `CentOS Stream8`, and `CentOS Stream9`, installed alongside the Linux kernel 5.15.

        Run the following commands:

        ```bash
        sudo ./scripts/setup_ice_irdma.sh
        sudo ./scripts/setup_build_env.sh
        ```

        Reboot the host after the scripts are executed.

    * **Option B** – The following method is universal and should work for the most Linux OS distributions.

        * XDP-tools with eBPF – Follow the [Simple guide](https://github.com/xdp-project/xdp-tools.git) for installation instructions.
        * MTL – Follow the [MTL setup guide](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/build.md).
        * E810 driver – Follow the [MTL NIC setup guide](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/e810.md).
        * gRPC – Refer to the [gRPC documentation](https://grpc.io/docs/languages/cpp/quickstart/) for installation instructions.
        * Install required packages

            * Ubuntu/Debian
                ```bash
                sudo apt-get update
                sudo apt-get install libbsd-dev cmake make rdma-core libibverbs-dev librdmacm-dev dracut
                ```
            * CentOS Stream
                ```bash
                sudo yum install -y libbsd-devel cmake make rdma-core libibverbs-devel librdmacm-devel dracut
                ```

        * Install the `irdma` driver and `libfabric`:

            ```bash
            ./scripts/setup_rdma_env.sh install
            ```
            For more information, see [Building and installing libfabric from source](https://github.com/ofiwg/libfabric?tab=readme-ov-file#building-and-installing-libfabric-from-source).

        * Reboot the host.

1. Build the Media Communications Mesh software components

    Run the build script

    ```bash
    ./build.sh
    ```

    This script builds and installs the following software components
    * SDK API library
       * File name: `libmcm_dp.so`
       * Header file to include: [`mesh_dp.h`](../../sdk/include/mesh_dp.h)
    * Media Proxy
       * Executable file name: `media_proxy`
    * Mesh Agent
       * Executable file name: `mesh-agent`

## Build for Docker

Make sure the Docker engine is configured and installed. It is recommended to use the Buildx toolkit.

1. Clone the repository

   ```bash
   git clone https://github.com/OpenVisualCloud/Media-Communications-Mesh.git
   ```

1. Navigate to the Media-Communications-Mesh directory

    ```bash
    cd Media-Communications-Mesh
    ```

1. Build Docker images

    Depending on the Docker installation, this step may require root privileges.

    Run the following script from the root directory of the repository to build all Dockerfiles

    ```bash
    ./build_docker.sh
    ```

    In case of success, the following Docker images will be available in the current Docker context:
    * `mcm/sample-app:latest`
    * `mcm/media-proxy:latest`
    * `mcm/ffmpeg:latest`
    * `mcm/ffmpeg:6.1-latest`

1. Check if Media Proxy can be run in a container by running the command below

    ```bash
    docker run --privileged -it -v /var/run/mcm:/run/mcm -v /dev/hugepages:/dev/hugepages mcm/media-proxy:latest
    ```

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
