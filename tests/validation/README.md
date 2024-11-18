# Media Communications Mesh SDK API validation

This part of the repository is to be used solely for holding validation applications for Media Communications Mesh API and other files related to validating mentioned API.

## Compilation process

> **Note:** At the moment, it is not automated to expedite the development process.

In order to compile the apps, first compile the whole Media Communications Mesh using `build.sh` script from the root of the repo, then use the following commands:

- `cc -o sender_app sender_app.c -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`
- `cc -o recver_app recver_app.c -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`

## Usage

Remember to properly start the Media Proxy application before running the sample apps in order to make the transmission possible.

Use `app_name -H` to see full usage options for a specific application.

### Common switches

> **Note:** Kept here with a best-effort approach and may be obsolete. Check usage help (`-H` or `--help`) for the newest information about present switches.

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
- `sudo MCM_MEDIA_PROXY_PORT=9992 ./sender -w 1920 -h 1080 -f 30 -x yuv422p10le" -s 192.168.96.1 -r 192.168.96.2 -i 9991 -b input.yuv`

Receiver:
- `sudo media_proxy -d 0000:c0:11.0 -t 9991`
- `sudo MCM_MEDIA_PROXY_PORT=9991 ./recver -w 1920 -h 1080 -f 30 -x yuv422p10le" -s 192.168.96.2 -p 9992 -b output.yuv`

## Validation helpers

An aside part with help information for validators.

### Media Proxy hugepages requirement

To meet Media Proxy hugepages requirement, as a super user, add a number >10 to each of the files:
```shell
/sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

Error thrown when the hugepages are not properly setup:
```text
EAL: No free 2048 kB hugepages reported on node 0
EAL: No free 2048 kB hugepages reported on node 1
EAL: No free 1048576 kB hugepages reported on node 0
EAL: No free 1048576 kB hugepages reported on node 1
```

### Media Proxy device vfio-pci mode

If a following message is presented

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
