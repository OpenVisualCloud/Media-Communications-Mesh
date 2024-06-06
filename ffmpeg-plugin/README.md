# FFmpeg plugin for MCM

## Build

### Prerequitites
Install dependencies and build MCM as described in the top level README.md, paragraph "Basic Installation".

### Build flow

1. Clone the FFmpeg repository and apply MCM patches
   ```bash
   ./clone-and-patch-ffmpeg.sh
   ```

1. Run the FFmpeg configuration tool
   ```bash
   ./configure-ffmpeg.sh
   ```

1. Build and install FFmpeg with the MCM plugin
   ```bash
   ./build-ffmpeg.sh
   ```

## Arguments
TBD

## Run

This test run demonstrates sending a video file from the 1st FFmpeg instance to the 2nd FFmpeg instance via MCM and then stream it to a remote machine via UDP.

### NIC setup
TBD

### Receiver side setup
1. Start media_proxy
   ```
   sudo media_proxy -d 0000:32:11.1 -i 192.168.96.2 -t 8002
   ```
1. Start FFmpeg to receive frames from MCM and stream to a remote machine via UDP
   ```
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -re -f mcm -frame_rate 24 -video_size nhd -pixel_format yuv420p -protocol_type auto -payload_type st20 -ip_addr 192.168.96.1 -port 9001 -i - -vcodec mpeg4 -f mpegts udp://<remote-ip>:<remote-port>
   ```

### Sender side setup
1. Start media_proxy
   ```
   sudo media_proxy -d 0000:32:11.0 -i 192.168.96.1 -t 8001
   ```
1. Start FFmpeg to stream a video file to the receiver via MCM
   ```
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <video-file-path> -f mcm -frame_rate 24 -video_size nhd -pixel_format yuv420p -protocol_type auto -payload_type st20 -ip_addr 192.168.96.2 -port 9001 -
   ```

### VLC player setup
On the remote machine start the VLC player and open a network stream from the next URL:
```
udp://@:1234
```

## Known Issues

### Shared libraries error:
FFmpeg build was successful but trying to run `ffmpeg` results in shared libraries error:
```
root@my-machine:~# ffmpeg

ffmpeg: error while loading shared libraries: libavfilter.so.9: cannot open shared object file: No such file or directory
```

**Resolution:**
Try running ffmpeg or exporting the `LD_LIBRARY_PATH` in your shell session by pointing to the `/usr/local/lib` folder. Examples:
```
root@my-machine:~# LD_LIBRARY_PATH=/usr/local/lib ffmpeg

ffmpeg version n6.1.1-152-ge821e6c21d Copyright (c) 2000-2023 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.4.0-1ubuntu1~22.04)
```
Using export:
```
root@my-machine:~# export LD_LIBRARY_PATH=/usr/local/lib
root@my-machine:~# ffmpeg

ffmpeg version n6.1.1-152-ge821e6c21d Copyright (c) 2000-2023 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.4.0-1ubuntu1~22.04)
```
