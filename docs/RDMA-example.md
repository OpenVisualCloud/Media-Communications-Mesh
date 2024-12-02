# RDMA example

This guide provides example commands that can be used to send a video file over RDMA on a single node. It has been created mainly for Debian users, but the commands for other OSs will be the same or similar.

## Prerequisites

1. You should have Media Communications Mesh built. Instruction can be found in [README](README.md#getting-started).

2. Libfabric should already be built and installed with RDMA-supporting providers.

  You can check if this is true with command:

  ```
  fi_info -p verbs -v
  ```

  If you see `fi_getinfo: -61 (No data available)`, make sure that before the installation of the libfabric:

  - irdma driver has been installed.

    ```
    lsmod | grep irdma
    ```
  - rdma-core packet have been installed.

    ```
    apt list rdma-core
    ```

  In case something is missing, please return to [README](README.md#getting-started).

3. Your RDMA-capable network interface has to have assigned IPv4 address.

  If it does not, you can print such interfaces with:

  ```
  rdma link show
  ```

  and then manually assign IPv4 address. Private networks range 192.168.\*.\*/16 is recommended:

  ```
  sudo ip addr add <IPv4> dev <interface>
  ```

4. You need to have video file prepared.

In case you don't have any video handy, you can create a dummy file as follows:

```
dd of=dummy_video.bin if=/dev/random bs=8294400 count=60
```

## Running sample apps

In this example you will run `4` separate processes, you can use `4` separate terminals for running them. This example sends `60` frames of a `1920x1080` raw video file in `yuv422p10le` video format. Media proxies are listening on ports `8002` and `8003`, the receiver is listening on port `9000`

- process number 1:

```
sudo media_proxy -t 8002
```

- process number 2:

```
sudo media_proxy -t 8003
```

- process number 3, replace \<IPv4> with the address of the RDMA-capable interface:

```
sudo MCM_MEDIA_PROXY_PORT=8002 ./out/bin/recver_app -r <IPv4> -t rdma -i 9000 -w 1920 -h 1080 -x yuv422p10le -b ./received_video.bin -o auto
```

- process number 4, replace \<IPv4> with the address of the RDMA-capable interface:

```
sudo MCM_MEDIA_PROXY_PORT=8003  ./out/bin/sender_app -p 9000 -s <IPv4> -t rdma -w 1920 -h 1080 -x yuv422p10le -b dummy_video.bin -n 60 -o auto
```

The file should be transferred via RDMA and saved into `received_video.bin`.
