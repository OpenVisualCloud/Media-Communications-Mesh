# FFmpeg plugin for MCM

![MCM FFmpeg Plugins](../docs/_static/ffmpeg-plugins-media-communications-mesh-1.webp)

## Build

### Prerequisites

Install dependencies and build MCM as described in the top level README.md, paragraph "Basic Installation".

### Build flow

1. Clone the FFmpeg repository (Release 7.0 by default) and apply MCM patches

   ```bash
   ./clone-and-patch-ffmpeg.sh
   ```

   **Note:** For FFmpeg Release 6.1, replace `7.0` with `6.1` in the above cloning script

2. Run the FFmpeg configuration tool

   ```bash
   ./configure-ffmpeg.sh
   ```

3. Build and install FFmpeg with the MCM plugin

   ```bash
   ./build-ffmpeg.sh
   ```

## MCM connection configuration

The next arguments are supported to configure a connection to MCM

| Argument        |  Type   | Description                                               |     Default      |
| --------------- | :-----: | --------------------------------------------------------- | :--------------: |
| `ip_addr`       | String  | Remote IP address                                         | `"192.168.96.1"` |
| `port`          | String  | Remote port (Sender), or Local port (Receiver)            |     `"9001"`     |
| `protocol_type` | String  | MCM Protocol type (`"auto"`, `"memif"`, etc.)             |     `"auto"`     |
| `payload_type`  | String  | Payload type (`"st20"`, `"st22"`, `"st30", "rdma"`, etc.) |     `"st20"`     |
| `socket_name`   | String  | Memif socket name                                         |        -         |
| `interface_id`  | Integer | Memif interface id                                        |       `0`        |

## Video configuration

The next arguments are supported to configure a video transmission

| Argument       |  Type  | Description                                     |     Default     |
| -------------- | :----: | ----------------------------------------------- | :-------------: |
| `video_size`   | String | Video frame size (`"640x480"`, `"hd720"`, etc.) |  `"1920x1080"`  |
| `pixel_format` | String | Video pixel format                              | `"yuv422p10le"` |
| `frame_rate`   | String | Video frame rate (`25`, `50`, `60`, etc.)       |      `25`       |

## Example – Run video transmission

This example demonstrates sending a video file from the 1st FFmpeg instance to the 2nd FFmpeg instance via MCM and then stream it to a remote machine via UDP.

### NIC setup

TBD

### Receiver side setup

1. Start media_proxy

   ```bash
   sudo media_proxy -d 0000:32:11.1 -i 192.168.96.2 -t 8002
   ```

2. Start FFmpeg to receive frames from MCM and stream to a remote machine via UDP

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -re -f mcm \
      -frame_rate 24 \
      -video_size nhd \
      -pixel_format yuv422p10le \
      -protocol_type auto \
      -payload_type st20 \
      -ip_addr 192.168.96.1 \
      -port 9001 \
      -i - -vcodec mpeg4 -f mpegts udp://<remote-ip>:<remote-port>
   ```

### Sender side setup

1. Start media_proxy

   ```bash
   sudo media_proxy -d 0000:32:11.0 -i 192.168.96.1 -t 8001
   ```

2. Start FFmpeg to stream a video file to the receiver via MCM

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <video-file-path> -f mcm \
      -frame_rate 24 \
      -video_size nhd \
      -pixel_format yuv422p10le \
      -protocol_type auto \
      -payload_type st20 \
      -ip_addr 192.168.96.2 \
      -port 9001 -
   ```

   When working with raw video files that lack metadata, you must explicitly provide FFmpeg with the necessary video frame details. This includes specifying the format `-f rawvideo`, pixel format `-pix_fmt`, and resolution `-s WxH`. For example:

   ```bash
   ffmpeg -f rawvideo -pix_fmt yuv422p10le -s 1920x1080 -i <video-file-path> ...
   ```

### VLC player setup

On the remote machine start the VLC player and open a network stream from the next URL:

```
udp://@:1234
```

## Audio configuration

The table below shows a proper way to configure the sender and the receiver depending on the audio PCM encoding format

| Audio encoding | Sender configuration            | Receiver configuration         |
| -------------- | ------------------------------- | ------------------------------ |
| PCM 16-bit     | Output device `mcm_audio_pcm16` | Input device `mcm_audio_pcm16` |
| PCM 24-bit     | Output device `mcm_audio_pcm24` | Input device `mcm_audio_pcm24` |

The next arguments are supported to configure an audio transmission

| Argument      |  Type   | Description                                                           | Default |
| ------------- | :-----: | --------------------------------------------------------------------- | :-----: |
| `channels`    | Integer | Number of audio channels (`1`, `2`, etc.)                             |   `2`   |
| `sample_rate` | Integer | Audio sample rate (`48000` or `96000`)                                | `48000` |
| `ptime`       | String  | Audio packet time according to ST2110-30 (the only option is `"1ms"`) | `"1ms"` |

## Example – Run audio transmission, PCM 24-bit

This example demonstrates sending a PCM 24-bit encoded audio file from the 1st FFmpeg instance to the 2nd FFmpeg instance via MCM.

### NIC setup

TBD

### Receiver side setup

1. Start media_proxy

   ```bash
   sudo media_proxy -d 0000:32:11.1 -i 192.168.96.2 -t 8002
   ```

2. Start FFmpeg to receive packets from MCM and store on the disk

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -re -f mcm_audio_pcm24 \
      -channels 2 \
      -sample_rate 48000 \
      -ptime 1ms \
      -protocol_type auto \
      -payload_type st30 \
      -ip_addr 192.168.96.1 \
      -port 9001 \
      -i - output.wav
   ```

### Sender side setup

1. Start media_proxy
   ```bash
   sudo media_proxy -d 0000:32:11.0 -i 192.168.96.1 -t 8001
   ```
2. Start FFmpeg to stream an audio file to the receiver via MCM

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <audio-file-path> -f mcm_audio_pcm24 \
      -channels 2 \
      -sample_rate 48000 \
      -ptime 1ms \
      -protocol_type auto \
      -payload_type st30 \
      -ip_addr 192.168.96.2 \
      -port 9001 -
   ```

## Known Issues

### Shared libraries error

FFmpeg build was successful but trying to run `ffmpeg` results in shared libraries error:

```bash
root@my-machine:~# ffmpeg

ffmpeg: error while loading shared libraries: libavfilter.so.9: cannot open shared object file: No such file or directory
```

**Resolution:**
Try running ffmpeg or exporting the `LD_LIBRARY_PATH` in your shell session by pointing to the `/usr/local/lib` folder. Examples:

```bash
root@my-machine:~# LD_LIBRARY_PATH=/usr/local/lib ffmpeg

ffmpeg version n6.1.1-152-ge821e6c21d Copyright (c) 2000-2023 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.4.0-1ubuntu1~22.04)
```

Using export:

```bash
root@my-machine:~# export LD_LIBRARY_PATH=/usr/local/lib
root@my-machine:~# ffmpeg

ffmpeg version n6.1.1-152-ge821e6c21d Copyright (c) 2000-2023 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.4.0-1ubuntu1~22.04)
```
