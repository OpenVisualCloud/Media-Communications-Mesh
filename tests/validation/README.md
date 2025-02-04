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

```text
functional
 +--- cluster ____ tests of multi-node RDMA-based transfers (simulated)
 |     +- ancillary __ ancillary data
 |     +- audio ______ raw audio data
 |     +- blob _______ any other data not mentioned elsewhere
 |     +- ffmpeg _____ ffmpeg plugin with all types of data
 |     '- video ______ raw and compressed video data
 +--- local ______ tests of single-node memory copy (memif)
 |     +- ancillary __ ancillary data
 |     +- audio ______ raw audio data
 |     +- blob _______ any other data not mentioned elsewhere
 |     +- ffmpeg _____ ffmpeg plugin with all types of data
 |     '- video ______ raw and compressed video data
 '--- st2110 _____ tests of ST 2210 standard with Media Transport Library
       +- ffmpeg _____ ffmpeg plugin with all types of data
       +- st20 _______ raw video (ST 2110-20)
       +- st22 _______ compressed video (ST 2110-22)
       '- st30 _______ raw audio (ST 2110-30)
```


## Development setup

```bash
cd tests/validation
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

For VSCode:

- set Python interpreter to: tests/validation/.venv/bin/python
- copy content of tests/validation/settings.json to .vscode/settings.json


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


## Listing virtual functions

Use `nicctl.sh list all` command to list all existing devices.

The script prints 6 columns:
- `ID` - an ordinal number for an interface (not static, see below)
- `PCI BDF` - a Bus:Device.Function describing a PCI device
- `Driver` - a driver used to communicate with the interface; `vfio-pci` for virtual functions
- `NUMA` - a NUMA node's number, assigned to the device
- `IOMMU` - a device's iommu group
- `IF Name` - interface name; except when virtual function, then `*`

Example of a list including 6 virtual functions (at the top):

```text
ID      PCI BDF         Driver          NUMA    IOMMU   IF Name
0       0000:c0:01.0    vfio-pci        1       346     *
1       0000:c0:01.1    vfio-pci        1       347     *
2       0000:c0:01.2    vfio-pci        1       348     *
3       0000:c0:01.3    vfio-pci        1       349     *
4       0000:c0:01.4    vfio-pci        1       350     *
5       0000:c0:01.5    vfio-pci        1       351     *
6       0000:32:00.0    ixgbe           0       31      eth2
7       0000:32:00.1    ixgbe           0       32      eth3
8       0000:c0:00.0    ice             1       201     eth0
9       0000:c0:00.1    ice             1       202     eth1
```

And a version with only physical functions enabled:

```text
ID      PCI BDF         Driver          NUMA    IOMMU   IF Name
0       0000:32:00.0    ixgbe           0       31      eth2
1       0000:32:00.1    ixgbe           0       32      eth3
2       0000:c0:00.0    ice             1       201     eth0
3       0000:c0:00.1    ice             1       202     eth1
```


## Disabling virtual functions

In order to disable all virtual functions for a selected physical interface, execute a following command:

```shell
nicctl.sh disable_vf <physical_device_pci_address>
```

For example, for eth0 interface from previous section, it is:

```shell
nicctl.sh disable_vf 0000:c0:00.0
```

> **Note:** Above sections about virtual functions were partially based on [Media Transport Library instruction](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md#321-create-intel-e810-vfs-and-bind-to-dpdk-pmd).

## Media Proxy hugepages requirement

> **Note:** [Minimal Media Transport Library setup](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md#4-setup-hugepage) specifies setting up 4 GB hugepages altogether. Below paragraph is just a recommendation, which should be used for testing purposes.

To ensure stable testing of Media Proxy, as super user, `echo 4 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages` (creating 4 x 1 GB hugepages) and `echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages` (creating 2048 x 2 MB hugepages).

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


## Proxy settings

Each address used within the tests (media_proxy included), should not be routed through a proxy.

Easiest solution is to add them to environment variable called `no_proxy`, by `export no_proxy=${no_proxy},<ip_address>`.


## RDMA

> **Note:** Current RDMA implementation allows only for simulated transfer (as of writing this section of the document).

In order to use the RDMA-based solution, the virtual functions on a used interface must be disabled.
