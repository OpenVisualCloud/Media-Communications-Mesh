
# RDMA example

This guide provides example commands that can be used to send a video file via RDMA on a single node.

## Prerequisites

1. Media Communications Mesh is built
```
./build.sh
```

2. Libfabric is build and installed with RDMA capable providers

    You can check if this is true with:
    ```
    fi_info -p verbs -v
    ```
    If nothing is listed make sure that before the installation of the libfabric:
    - irdma driver has been installed
        ```
        lsmod | grep irdma
        ```
    - libibverbs-dev and librdmacm-dev packets have been installed
        ```
        apt list libibverbs-dev librdmacm-dev
        ```
    In case something is missing, please return to [README](README.md#getting-started)

3. Your RDMA capable interface has to have assigned IPv4 address

    If it does not, you can print such interfaces with:
    ```
    ibv_devinfo
    ```
    and then manually assign IPv4 address:
    ```
    sudo ip addr add <IPv4> dev <interface>
    ```

4. Video file exists

In case you don't have any video handy, you can create a dummy file as follows:
```
dd of=dummy_video.bin if=/dev/random bs=8294400 count=60
```

## Running sample apps

You need 4 terminals to be able to run this example

- terminal number 1:
```
sudo ./out/bin/media_proxy -t 8002
```
- terminal number 2:
```
sudo ./out/bin/media_proxy -t 8003
```
- terminal number 3, replace \<IPv4> with the address of the RDMA-capable interface:
```
sudo MCM_MEDIA_PROXY_PORT=8002 ./out/bin/recver_app -r <IPv4> -t rdma -i 9000 -w 1920 -h 1080 -x yuv422p10le -b ./received_video.bin -o auto
```
- terminal number 4, replace \<IPv4> with the address of the RDMA-capable interface:
```
sudo MCM_MEDIA_PROXY_PORT=8003  ./out/bin/sender_app -p 9000 -s <IPv4> -t rdma -w 1920 -h 1080 -x yuv422p10le -b dummy_video.bin -n 60 -o auto
```

The file should be transferred via RDMA and saved into received_video.bin