# Media Communications Mesh SDK API validation

This part of the repository is to be used solely for holding validation applications for Media Communications Mesh API and other files related to validating mentioned API.

## Compilation process

> **Note:** At the moment, it is not automated to expedite the development process.

In order to compile the apps, first compile the whole Media Communications Mesh using `build.sh` script from the root of the repo, then use the following commands:

- `cc -o sender_val sender_val.c -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`
- `cc -o recver_val recver_val.c -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`

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
2. Use a `nicctl.sh` script from Media Transport Library to create the VFs `${mtl}/script/nicctl.sh create_vf <pci_bus_address>`, e.g. `${mtl}/script/nicctl.sh create_vf 0000:c0:00.1`.
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


## Usage

Remember to properly start the Media Proxy application before running the sample apps in order to make the transmission possible.

### Common switches

> **Note:** Kept here with a best-effort approach and may be obsolete. Check usage help (`-H` or `--help`) for the newest information about present switches. Use `app_name -H` to see full usage options for a specific application.

| Switch | Long switch       | Variable type      | Argument value         | Default                        | Examples                   |
| ------ | ----------------- | ------------------ | ---------------------- | ------------------------------ | -------------------------- |
| `-H`   | `--help`          | N/A                | empty (no argument)    | N/A                            | N/A                        |
| `-b`   | `--file_name`     | `static char[128]` | Input/Output file name | `""` (empty)                   | `"in.yuv"` / `"out.yuv"`   |
| `-w`   | `--width`         | `uint32_t`         | Video width [pixels]   | `1920`                         | `1920`                     |
| `-h`   | `--height`        | `uint32_t`         | Video height [pixels]  | `1080`                         | `1080`                     |
| `-f`   | `--fps`           | `double`           | Frames per second      | `30.0`                         | `25`                       |
| `-x`   | `--pix_fmt`       | `char[32]`         | Video pixel format     | `"yuv422p10le"`                | `"yuv444p"`                |
| `-r`   | `--rcv_ip`        | `char[253]`        | Receiver IP address    | `"127.0.0.1"`                  | `"192.168.96.1"`           |
| `-i`   | `--rcv_port`      | `char[5]`          | Receiver port number   | `9001`                         | `9001`                     |
| `-s`   | `--send_ip`       | `char[253]`        | Sender IP address      | `"127.0.0.1"`                  | `"192.168.96.2"`           |
| `-p`   | `--send_port`     | `char[5]`          | Sender port number     | `9001`                         | `9002`                     |
| `-o`   | `--protocol_type` | `char[32]`         | Type of protocol       | `"auto"`                       | `"memif"`                  |
| `-t`   | `--payload_type`  | `char[32]`         | Type of payload        | `"st20"`                       | `"rdma"`/`"st22"`/`"st30"` |
| `-k`   | `--socketpath`    | `char[108]`        | Socket path            | `"/run/mcm/mcm_rx_memif.sock"` | `"/other/path.sock"`       |
| `-d`   | `--interfaceid`   | `uint32_t`         | ID of an interface     | `0`                            | `1`                        |
| `-l`   | `--loop`          | `bool`             | Whether to loop or not | `0` (false)                    | `1` (true)                 |

### Usage examples

Sender:
- `sudo media_proxy -d 0000:c0:11.1 -t 9992`
  ```text
  DEBUG: Set MTL configure file path to /usr/local/etc/imtl.json
  INFO: TCP Server listening on 0.0.0.0:9992
  ```
- `sudo MCM_MEDIA_PROXY_PORT=9992 ./sender_val -w 1920 -h 1080 -f 30 -x yuv422p10le -s 192.168.96.1 -r 192.168.96.2 -i 9991 -b input.yuv`
  ```text
  INFO - Set default media-proxy IP address: 127.0.0.1
  INFO - Connecting to MCM Media-Proxy: 127.0.0.1:9992
  INFO - Connected to media-proxy.
  INFO - Session ID: 1
  INFO - Create memif socket.
  INFO - Create memif interface.
  MEMIF DETAILS
  ==============================
          interface name: memif_tx_0
          app name: memif_tx_0
          remote interface name: memif_tx_0
          remote app name: memif_tx_0
          id: 0
          secret: (null)
          role: slave
          mode: ethernet
          socket path: /run/mcm/media_proxy_tx_0.sock
          regions num: 2
                  regions idx: 0
                  regions addr: 0x7f7d7f4bc000
                  regions size: 384
                  regions ext: 0
                  regions idx: 1
                  regions addr: 0x7f7d7b2b2000
                  regions size: 66355200
                  regions ext: 0
          rx queues:
                  queue id: 0
                  ring size: 4
                  buffer size: 8294400
          tx queues:
                  queue id: 0
                  ring size: 4
                  buffer size: 8294400
          link: up
  INFO - memif connected!
  INFO - Success connect to MCM media-proxy.
  INFO: frame_size = 8294400
  TX frames: [0], FPS: 0.00 [30.00]
  Throughput: 0.00 MB/s, 0.00 Gb/s
  pacing: 33333
  spend: 5549

  (...)

  INFO: frame_size = 8294400
  TX frames: [49], FPS: 30.02 [30.00]
  Throughput: 248.97 MB/s, 1.99 Gb/s
  pacing: 33333
  spend: 33476

  INFO: frame_size = 8294400
  INFO - memif disconnected!
  ```

Receiver:
- `sudo media_proxy -d 0000:c0:11.0 -t 9991`
  ```text
  DEBUG: Set MTL configure file path to /usr/local/etc/imtl.json
  INFO: TCP Server listening on 0.0.0.0:9991
  ```
- `sudo MCM_MEDIA_PROXY_PORT=9991 ./recver_val -w 1920 -h 1080 -f 30 -x yuv422p10le -s 192.168.96.2 -p 9992 -b output.yuv`
  ```text
  INFO - Set default media-proxy IP address: 127.0.0.1
  INFO - Connecting to MCM Media-Proxy: 127.0.0.1:9991
  INFO - Connected to media-proxy.
  INFO - Session ID: 1
  INFO - Create memif socket.
  INFO - Create memif interface.
  MEMIF DETAILS
  ==============================
          interface name: memif_rx_0
          app name: memif_rx_0
          remote interface name: memif_rx_0
          remote app name: memif_rx_0
          id: 0
          secret: (null)
          role: slave
          mode: ethernet
          socket path: /run/mcm/media_proxy_rx_0.sock
          regions num: 2
                  regions idx: 0
                  regions addr: 0x7f4b337b0000
                  regions size: 384
                  regions ext: 0
                  regions idx: 1
                  regions addr: 0x7f4b2f5a6000
                  regions size: 66355200
                  regions ext: 0
          rx queues:
                  queue id: 0
                  ring size: 4
                  buffer size: 8294400
          tx queues:
                  queue id: 0
                  ring size: 4
                  buffer size: 8294400
          link: up
  INFO - memif connected!
  INFO - Success connect to MCM media-proxy.
  INFO: buf->len = 8294400 frame size = 8294400
  RX frames: [0], latency: 0.0 ms, FPS: 1192.027
  Throughput: 9887.15 MB/s, 79.10 Gb/s

  (...)
  INFO: buf->len = 8294400 frame size = 8294400
  RX frames: [49], latency: 0.0 ms, FPS: 30.057
  Throughput: 249.31 MB/s, 1.99 Gb/s

  Connection closed
  INFO - memif disconnected!
  ```

## Validation helpers

An aside part with help information for validators.

### Media Proxy hugepages requirement

> **Note:** [Minimal Media Transport Library setup](https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/doc/run.md#4-setup-hugepage) specifies setting up 4 GB hugepages altogether. Below paragraph is just a recommendation, which should be used for testing purposes.

To ensure stable testing of Media Proxy, as super user, add a number 4 to `/sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages` (4 * 1 GB hugepages) and 2048 to `/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages` (2048 * 2 MB hugepages).

Error thrown when the hugepages are not properly setup:
```text
EAL: No free 2048 kB hugepages reported on node 0
EAL: No free 2048 kB hugepages reported on node 1
EAL: No free 1048576 kB hugepages reported on node 0
EAL: No free 1048576 kB hugepages reported on node 1
```

### Media Proxy device vfio-pci mode

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
