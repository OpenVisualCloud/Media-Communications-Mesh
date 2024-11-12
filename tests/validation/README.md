# Media Communications Mesh API validation

This part of the repository is to be used solely for holding validation applications for Media Communications Mesh API and other files related to validating mentioned API.

## Compilation process

> **Note:** At the moment, it is not automated to expedite the development process.

In order to compile the apps, first compile the whole Media Communications Mesh using `build.sh` script from the root of the repo, then use following commands:

`cc -o <output_name> <input_file.c> -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`

Examples:
- `cc -o video_sender_app video_sender_app.c -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`
- `cc -o video_recver_app video_recver_app.c -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`

## Usage

Remember to properly start the Media Proxy application before running the sample apps, in order to make the transmission possible.

Use `app_name -H` to see full usage options for a specific application.

Common switches:
TBD
<!-- TODO: Add switches here at the end, when everything is stable
- `-z <filename>` - input/output filename
- `-a <remote_ip_addr>` - remote IP address
- `-p <remote_port>` - remote port
- `-l <local_ip_addr>` - local IP address
- `-o <local_port>` - local port
- `-t <st20|st22|st30>` - video transport type (ST2110-20 = "st20" raw; ST2110-22 - "st22" compressed)
^^^^^^TODO: Consider changing that
- `-w <pix_num>` - video width
- `-h <pix_num>` - video height
- `-f <fps_val>` - video frames per second (fps)
- `-x <pix_fmt>` - video pixel format
-->

Usage examples:
- `./video_sender_app -z 1080p_yuv422_10b_1.yuv -a 192.168.96.1 -p 9001 -l 192.168.96.2 -o 9002 -t st20 -w 1920 -h 1080 -f 30 -x yuv422p10le`
- `./video_recver_app -z out_1080p_yuv422_10b_1.yuv -a 192.168.96.2 -p 9002 -l 192.168.96.1 -o 9001 -t st20 -w 1920 -h 1080 -f 30 -x yuv422p10le`

## Known issues

### Media Proxy hugepages requirement

To meet Media Proxy hugepages requirement, as super user, add a number >10 to each of the files:
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
