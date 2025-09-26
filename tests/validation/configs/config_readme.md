# Topology Configuration File Guide

This document describes the structure and fields of the `topology.yaml` and `test_config.yaml` files used for Media Communications Mesh (MCM) validation. Use this as a reference when creating or editing your topology configuration.

---

## Topology Structure Overview

```yaml
metadata:
  version: '2.4'
hosts:
  - name: mesh-agent
    instantiate: true
    role: sut
    network_interfaces:
      - interface_name: <interface name>
    connections:
      - ip_address: <ip address>
        connection_type: SSHConnection
        connection_options:
          port: {{ port|default(22) }}
          username: <username>
          password: <password>
    extra_info:
      mtl_path: /opt/intel/mtl
      nicctl_path: /opt/intel/mtl/script
      filepath: /mnt/media/
      output_path: /home/gta/received/
      ffmpeg_path: /opt/intel/_build/ffmpeg-7.0/ffmpeg-7-0_mcm_build/bin/ffmpeg
      prefix_variables:
        LD_LIBRARY_PATH: /opt/intel/_build/ffmpeg-7.0/ffmpeg-7-0_mcm_build/lib
        NO_PROXY: 127.0.0.1,localhost
        no_proxy: 127.0.0.1,localhost
      media_proxy:
        st2110: true
        sdk_port: 8002
        st2110_dev: <st2110_dev vf pci address>
        st2110_ip: <st2110 vf ip address>
        rdma: true
        rdma_ports: 9100-9999
        rdma_ip: <rdma pf ip address>
      mesh_agent:
        control_port: 8100
        proxy_port: 50051
```

---

## Field Descriptions

### metadata
- **version**: Version of the topology file format.

### hosts
A list of host definitions. Each host can have the following fields:

- **name**: Name of the host.
    If you want to run `mesh-agent` in your test, name one host as `mesh-agent`or specify in test_config.yaml name of one of your hosts as `mesh_agent_name: name_of_chosen_host`. This is required for the system to recognize it as the mesh-agent host.
    If you want to use external mesh-agent set its IP address and ports in test_config.yaml.
- **instantiate**: Set to `true` to instantiate this host and create a connection.
- **role**: Role of the host (supported only `sut` or `client`).
- **network_interfaces**: List of network interfaces used in the test.
  - **interface_name**: Name of the network interface (e.g., `eth1`). You can also specify by PCI device ID or address. Please read MFD Config readme for more details on how to specify the interface. mfd-config repo: [https://github.com/intel-innersource/libraries.python.mfd.pytest-mfd-config](https://github.com/intel-innersource/libraries.python.mfd.pytest-mfd-config)
- **connections**: List of connections for the host.
  - **ip_address**: IP address for the connection.
  - **connection_type**: Type of connection (e.g., `SSHConnection`) please read MFD Connect readme for more details: [https://github.com/intel-innersource/libraries.python.mfd.mfd-connect/tree/main/mfd_connect](https://github.com/intel-innersource/libraries.python.mfd.mfd-connect/tree/main/mfd_connect)
  - **connection_options**: Options for the connection.
    - **port**: SSH port (default: 22).
    - **username**: SSH username.
    - **password**: SSH password.
- **extra_info**: Additional configuration for the host.
  - **integrity_path**: Path for integrity scripts on the host if you want to run integrity tests (optional).
  - **mtl_path**: Custom path to the MTL repo (optional) default is /mcm_path/_build/mtl.
  - **nicctl_path**: Path to `nicctl.sh` script (optional).
  - **filepath**: Path to input media files for transmitter (e.g., `/mnt/media/`).
  - **output_path**: Path where output files will be stored (e.g., `/home/gta/received/`).
  - **ffmpeg_path**: Path to FFmpeg binary (e.g., `/opt/intel/_build/ffmpeg-7.0/ffmpeg-7-0_mcm_build/bin/ffmpeg`).
  - **prefix_variables**: Environment variables to set before running FFmpeg.
    - **LD_LIBRARY_PATH**: Path to FFmpeg libraries.
    - **NO_PROXY**, **no_proxy**: Proxy bypass settings.
  - **media_proxy**: (Optional) Media proxy configuration. DO NOT set this if you don't want to run media proxy process.
    - **st2110**: Set to `true` to use the ST2110 bridge.
    - **sdk_port**: Port for media proxy SDK.
    - **st2110_dev**: (Optional) PCI device ID of the ST2110 bridge.
    - **st2110_ip**: (Optional) IP address of the ST2110 bridge.
    - **rdma**: Set to `true` to use RDMA in your test.
    - **rdma_ports**: (Optional) Range of RDMA ports for media proxy.
    - **rdma_ip**: (Optional) IP address of the RDMA interface.
  - **mesh_agent**: (Optional) Override default mesh-agent values.
    - **control_port**: Control port for mesh-agent (default: 8100).
    - **proxy_port**: Proxy port for mesh-agent (default: 50051).

---

## Notes

- Fields marked as optional can be omitted if default values are sufficient.
- You can add more hosts or network interfaces as needed.
- If a field is not set, the system will use default values or those set in the OS.
- FFmpeg and file path configurations are now recommended to be defined directly in the host's `extra_info` section rather than using the previously nested `tx` and `rx` structure, which has been deprecated.

---

---
## Test config Structure Overview

```yaml
# Mesh agent configuration (if not defined in topology)
mesh-agent:
  control_port: 8100
  proxy_port: 50051

# ST2110 configuration (if not defined in topology)
st2110:
  sdk: 8002

# RDMA configuration (if not defined in topology)
rdma:
  rdma_ports: 9100-9999

# Global path configurations (if not defined in host's extra_info)
# Note: It's now recommended to define these in the host's extra_info section
# of the topology file for better organization and host-specific settings
input_path: "/opt/intel/input_path/"

# Build configurations
mcm_ffmpeg_rebuild: false  # Set to true to rebuild FFmpeg with MCM plugin
mcm_ffmpeg_version: "7.0"  # Default FFmpeg version for MCM

mtl_ffmpeg_rebuild: false  # Set to true to rebuild FFmpeg with MTL plugin
mtl_ffmpeg_version: "7.0"  # Default FFmpeg version for MTL
mtl_ffmpeg_gpu_direct: false  # Set to true to enable GPU direct support

rx_tx_app_rebuild: false  # Set to true to rebuild TestApp

# Log configurations
keep_logs: true
```

## Directory Structure

### `/opt/intel`

This directory contains the main components and dependencies for Media Communications Mesh development and testing:

- `_build/`
  - `mcm/`
    - `bin/`
      - `media_proxy`
      - `mesh-agent`
    - `lib/`
      - `libmcm_dp.so.*`
  - `ffmpeg-6-1/`
    - `ffmpeg-6-1_mcm_build/`
      - `ffmpeg` – MCM-patched FFmpeg 6.1 binary
      - `lib/` – Libraries for MCM-patched FFmpeg 6.1
    - `ffmpeg-6-1_mtl_build/`
      - `ffmpeg` – MTL-patched FFmpeg 6.1 binary
      - `lib/` – Libraries for MTL-patched FFmpeg 6.1
  - `ffmpeg-7-0/`
    - `ffmpeg-7-0_mcm_build/`
      - `ffmpeg` – MCM-patched FFmpeg 7.0 binary (DEFAULT_MCM_FFMPEG7_PATH)
      - `lib/` – Libraries for MCM-patched FFmpeg 7.0 (DEFAULT_MCM_FFMPEG7_LD_LIBRARY_PATH)
    - `ffmpeg-7-0_mtl_build/`
      - `ffmpeg` – MTL-patched FFmpeg 7.0 binary
      - `lib/` – Libraries for MTL-patched FFmpeg 7.0
  - ... (other build outputs)
- `ffmpeg/` – FFmpeg repository
- `mcm/` – Media Communications Mesh repository
- `mtl/` – Media Transport Library (MTL) repository
- `integrity/` – Integrity check tools or files
- `input_path/` – Directory for input files used during validation (for transmitters tx)
- `output_path/` – Directory where output files are stored during validation (for receivers rx)
- `openh264/` – OpenH264 codec binaries and libraries

### `/mnt/media`

This directory is typically used as a mount point for media files required during validation and testing. It may contain large video/audio files or test vectors.

## Path Constants in Engine/const.py

Key path constants defined in `Engine/const.py`:

- `INTEL_BASE_PATH = "/opt/intel"` - Base path for all Intel software
- `MCM_PATH = "/opt/intel/mcm"` - Path to the MCM repository
- `MTL_PATH = "/opt/intel/mtl"` - Path to the MTL repository
- `MCM_BUILD_PATH = "/opt/intel/_build/mcm"` - Path for MCM built binaries
- `MTL_BUILD_PATH = "/opt/intel/_build/mtl"` - Path for MTL built binaries
- `DEFAULT_FFMPEG_PATH = "/opt/intel/ffmpeg"` - Path to the FFmpeg repository
- `DEFAULT_OPENH264_PATH = "/opt/intel/openh264"` - Path to the OpenH264 installation
- `ALLOWED_FFMPEG_VERSIONS = ["6.1", "7.0"]` - Supported FFmpeg versions
- `DEFAULT_MCM_FFMPEG_VERSION = "7.0"` - Default FFmpeg version for MCM
- `DEFAULT_MCM_FFMPEG_PATH = "/opt/intel/_build/ffmpeg-7.0/ffmpeg-7-0_mcm_build"` - Path to the MCM FFmpeg build
- `DEFAULT_MCM_FFMPEG_LD_LIBRARY_PATH = "/opt/intel/_build/ffmpeg-7.0/ffmpeg-7-0_mcm_build/lib"` - Library path for MCM FFmpeg
- `DEFAULT_MTL_FFMPEG_VERSION = "7.0"` - Default FFmpeg version for MTL
- `DEFAULT_MTL_FFMPEG_PATH = "/opt/intel/_build/ffmpeg-7.0/ffmpeg-7-0_mtl_build"` - Path to the MTL FFmpeg build
- `DEFAULT_MTL_FFMPEG_LD_LIBRARY_PATH = "/opt/intel/_build/ffmpeg-7.0/ffmpeg-7-0_mtl_build/lib"` - Library path for MTL FFmpeg
- `DEFAULT_MEDIA_PATH = "/mnt/media/"` - Path to the media files for testing
- `DEFAULT_INPUT_PATH = "/opt/intel/input_path"` - Path for input files
- `DEFAULT_OUTPUT_PATH = "/opt/intel/output_path"` - Path for output files

## Using the Test Configuration

To use a specific configuration file, pass it to pytest using the `--test-config` option:

```bash
pytest --test-config=configs/test_config.yaml
```

## Build FFmpeg with Plugins

To build FFmpeg with the MCM or MTL plugin, set the corresponding rebuild flag to `true` in your test configuration:

```yaml
mcm_ffmpeg_rebuild: true
mcm_ffmpeg_version: "7.0"

mtl_ffmpeg_rebuild: true
mtl_ffmpeg_version: "7.0"
mtl_ffmpeg_gpu_direct: false
```


## Field Descriptions
- **mesh_agent_name**: Name of the mesh-agent host. If not specified, defaults to `mesh-agent`. This is the host designated in the topology file to run the mesh-agent.
- **mesh_ip**: IP address of the mesh-agent. If you want to use an external mesh-agent, specify its IP address here.
- **mesh_port**: Port of the mesh-agent. If you want to use an external mesh-agent, specify its port here. Default is `50051`.
- **control_port**: Control port of the mesh-agent. If you want to use an external mesh-agent, specify its control port here. Default is `8100`.
