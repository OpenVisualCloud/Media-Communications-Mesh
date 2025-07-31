# Media Communications Mesh SDK API validation

This part of the repository is to be used solely for holding validation applications for Media Communications Mesh API and other files related to validating mentioned API.

## Framework

Framework is based on Pytest.

It was improved with:

- additional logging capabilities, each test case has its own log file.
- all previous logs are kept in logs folder,
- log files are colored green (PASS) and red (FAIL),
- additional log levels were added to keep logs of:
  - TESTCMD - command used to execute the actual test case,
  - CMD - additional commands required by test case for setup or result validation,
  - STDOUT - stdout of commands,
  - DMESG - dmesg generated during test case execution,
  - RESULT - human readable result output,
- ability to mark the test cases as xfails and assign them to bug tickets,
- ability to mark the test cases as covering the requirements,
- generation of logs/latest/report.csv with results, test commands and other result info.

Project uses flake8, black, isort and markdownlint as linters. All of these are available as VSCode extenstions.

- pyproject.toml - black configuration
- setup.cfg - flake8 configuration
- settings.json - example settings for VSCode, to be copied to .vscode/settings.json


## Folder structure

> **Note:** Some of the folders mentioned below may be unavailable in the current version of the repository.

- **`functional`**
  - **`cluster`** – tests of multi-node RDMA-based transfers (simulated)
    - `ancillary` – ancillary data
    - `audio` – raw audio data
    - `blob` – any other data not mentioned elsewhere
    - `ffmpeg` – ffmpeg plugin with all types of data
    - `video` – raw and compressed video data
  - **`local`** – tests of single-node memory copy (memif)
    - `ancillary` – ancillary data
    - `audio` – raw audio data
    - `blob` – any other data not mentioned elsewhere
    - `ffmpeg` – ffmpeg plugin with all types of data
    - `video` – raw and compressed video data
  - **`st2110`** – tests of ST 2210 standard with Media Transport Library
    - `ffmpeg` – ffmpeg plugin with all types of data
    - `st20` – raw video (ST 2110-20)
    - `st22` – compressed video (ST 2110-22)
    - `st30` – raw audio (ST 2110-30)


## Development setup

```bash
cd tests/validation
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

#### Example: Manual Test Run

To manually run the `test_blob_25_03` test from `test_blob_25_03.py` with the parameter `file = random_bin_100M`, use:

```bash
sudo .venv/bin/python3 -m pytest \
  --topology_config=./configs/topology_config_workflow.yaml \
  --test_config=./configs/test_config_workflow.yaml \
  ./functional/local/blob/test_blob_25_03.py::test_blob_25_03[|file = random_bin_100M|]
```

To collect all smoke tests use:

```bash
sudo .venv/bin/python3 -m pytest --collect-only --quiet ./functional/ -m smoke
```

### VSCode Setup

- Set the Python interpreter to: `tests/validation/.venv/bin/python`
- Copy the contents of `tests/validation/settings.json` to `.vscode/settings.json`
- Configure your settings as needed, following the instructions in [configs/config_readme.md](./configs/config_readme.md)
- Example test configuration arguments:
  ```yml
  --test_config=./tests/validation/configs/test_config.yaml
  --topology_config=./tests/validation/configs/topology_config.yaml
  ```

## Development Folder Structure

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

```
/mnt/media/
└── [media files and test assets]
```

All paths can be adjusted in configs.

## Creating virtual functions

In order to create proper virtual functions (VFs):

1. Find addresses of 800-series (E810) card network interfaces with `lshw -C network`:

    ```text
    (...)
    *-network:0
         description: Ethernet interface
         product: Ethernet Controller E810-C for QSFP
         vendor: Intel Corporation
         physical id: 0
         bus info: pci@0000:c0:00.0
         logical name: eth0
         version: 02
         serial: 6c:fe:54:5a:18:70
         capacity: 25Gbit/s
         width: 64 bits
         clock: 33MHz
         capabilities: bus_master cap_list rom ethernet physical 25000bt-fd autonegotiation
         configuration: autonegotiation=off broadcast=yes driver=ice driverversion=Kahawai_1.14.  9_20240613 firmware=4.60 0x8001e8b1 1.3682.0 latency=0 link=no multicast=yes  # link=no!
         resources: iomemory:2eff0-2efef iomemory:2eff0-2efef irq:16   memory:2efffa000000-2efffbffffff memory:2efffe010000-2efffe01ffff   memory:f1600000-f16fffff memory:2efffd000000-2efffdffffff   memory:2efffe220000-2efffe41ffff
    *-network:1
         description: Ethernet interface
         product: Ethernet Controller E810-C for QSFP
         vendor: Intel Corporation
         physical id: 0.1
         bus info: pci@0000:c0:00.1
         logical name: eth1
         version: 02
         serial: 6c:fe:54:5a:18:71
         capacity: 25Gbit/s
         width: 64 bits
         clock: 33MHz
         capabilities: bus_master cap_list rom ethernet physical fibre 25000bt-fd autonegotiation
         configuration: autonegotiation=on broadcast=yes driver=ice driverversion=Kahawai_1.14.  9_20240613 duplex=full firmware=4.60 0x8001e8b1 1.3682.0 latency=0 link=yes   multicast=yes # link=yes!
         resources: iomemory:2eff0-2efef iomemory:2eff0-2efef irq:16   memory:2efff8000000-2efff9ffffff memory:2efffe000000-2efffe00ffff   memory:f1500000-f15fffff memory:2efffc000000-2efffcffffff   memory:2efffe020000-2efffe21ffff
    (...)
    ```

2. Note the address of a PCI bus for the interface to be used as a VF-bound. Use only interfaces with `link=yes`, like `eth1` above.
3. Use a `nicctl.sh` script from Media Transport Library to create the VFs `${mtl}/script/nicctl.sh create_vf <pci_bus_address>`, e.g. `${mtl}/script/nicctl.sh create_vf 0000:c0:00.1`.

    ```text
    0000:c0:00.1 'Ethernet Controller E810-C for QSFP 1592' if=eth1 drv=ice unused=vfio-pci
    Bind 0000:c0:11.0(eth4) to vfio-pci success
    Bind 0000:c0:11.1(eth5) to vfio-pci success
    Bind 0000:c0:11.2(eth6) to vfio-pci success
    Bind 0000:c0:11.3(eth7) to vfio-pci success
    Bind 0000:c0:11.4(eth8) to vfio-pci success
    Bind 0000:c0:11.5(eth9) to vfio-pci success
    Create 6 VFs on PF bdf: 0000:c0:00.1 eth1 succ
    ```

This section was partially based on [Media Transport Library instruction](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md#321-create-intel-e810-vfs-and-bind-to-dpdk-pmd).

## Media Proxy hugepages requirement

> **Note:** [Minimal Media Transport Library setup](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md#4-setup-hugepage) specifies setting up 4 GB hugepages altogether. Below paragraph is just a recommendation, which should be used for testing purposes.

To ensure stable testing of Media Proxy, as super user, `echo 4 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages` (creating 4 *1 GB hugepages) and `echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages` (creating 2048* 2 MB hugepages).

Error thrown when the hugepages are not properly setup:

```text
EAL: No free 2048 kB hugepages reported on node 0
EAL: No free 2048 kB hugepages reported on node 1
EAL: No free 1048576 kB hugepages reported on node 0
EAL: No free 1048576 kB hugepages reported on node 1
```

## Media Proxy device vfio-pci mode

If the following message is presented

```text
MTL: (...), Error: Run "dpdk-devbind.py -s | grep Ethernet" to check if other port driver is ready as vfio-pci mode
```

run the command mentioned (`dpdk-devbind.py -s | grep Ethernet`) and find the network interface of interest. Example:

```text
$ dpdk-devbind.py -s | grep Ethernet
0000:c0:01.0 'Ethernet Adaptive Virtual Function 1889' drv=vfio-pci unused=iavf
0000:c0:01.1 'Ethernet Adaptive Virtual Function 1889' drv=vfio-pci unused=iavf
0000:32:00.0 'Ethernet Controller 10G X550T 1563' if=eth2 drv=ixgbe unused=vfio-pci *Active*
0000:c0:00.0 'Ethernet Controller E810-C for QSFP 1592' if=eth0 drv=ice unused=vfio-pci 
```

Switch the virtual interface's binding to vfio-pci with `dpdk-devbind.py -b vfio-pci <pci_address>`. For example, `dpdk-devbind.py -b vfio-pci 0000:c0:00.0`.

Switch the virtual interface's binding to vfio-pci with `dpdk-devbind.py -b vfio-pci <pci_address>`. For example, `dpdk-devbind.py -b vfio-pci 0000:c0:00.0`.

