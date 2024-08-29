# FFmpeg plugin for MCM

## Build

### Prerequitites

Install dependencies and build MCM as described in the top level README.md, paragraph "Basic Installation".

### Build flow

1. Clone the FFmpeg repository and apply MCM patches

   ```bash
   ./clone-and-patch-ffmpeg.sh
   ```

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

| Argument        | Type    | Description                                              | Default          |
| --------------- | :-----: | -------------------------------------------------------- | :--------------: |
| `ip_addr`       | String  | Remote IP address                                        | `"192.168.96.1"` |
| `port`          | String  | Remote port (Sender), or Local port (Receiver)           | `"9001"`         |
| `protocol_type` | String  | MCM Protocol type (`"auto"`, `"memif"`, etc.)            | `"auto"`         |
| `payload_type`  | String  | ST2110 payload type (`"st20"`, `"st22"`, `"st30"`, etc.) | `"st20"`         |
| `socket_name`   | String  | Memif socket name                                        | -                |
| `interface_id`  | Integer | Memif interface id                                       | `0`              |

## Video configuration

The next arguments are supported to configure a video transmission

| Argument       | Type   | Description                                     | Default         |
| -------------- | :----: | ----------------------------------------------- | :-------------: |
| `video_size`   | String | Video frame size (`"640x480"`, `"hd720"`, etc.) | `"1920x1080"`   |
| `pixel_format` | String | Video pixel format                              | `"yuv422p10le"` |
| `frame_rate`   | String | Video frame rate (`25`, `50`, `60`, etc.)       | `25`            |

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

### VLC player setup

On the remote machine start the VLC player and open a network stream from the next URL:
```
udp://@:1234
```

## Audio configuration

The table below shows a proper way to configure the sender and the receiver depending on the audio PCM encoding format

| Audio encoding | Sender configuration | Receiver configuration |
| --- | --- | --- |
| PCM 24-bit | Output device `mcm_audio` | Input device `mcm_audio` and argument `-pcm_fmt pcm24`
| PCM 16-bit | Output device `mcm_audio_pcm16` | Input device `mcm_audio` and argument `-pcm_fmt pcm16`

The next arguments are supported to configure an audio transmission

| Argument      | Type    | Description                                      | Default   |
| ------------- | :-----: | ------------------------------------------------ | :-------: |
| `channels`    | Integer | Number of audio channels (`1`, `2`, etc.)        | `2`       |
| `sample_rate` | Integer | Audio sample rate (`44100`, `48000`, or `96000`) | `48000`   |
| `ptime`       | String  | MTL audio packet time (`"1ms"` or `"125us"`)     | `"1ms"`   |
| `pcm_fmt`     | String  | PCM audio format (`"pcm24"` or `"pcm16"`)        | `"pcm24"` |

## Example – Run audio transmission

This example demonstrates sending an audio file from the 1st FFmpeg instance to the 2nd FFmpeg instance via MCM.

There are two options of configuration:
* **Option A** for PCM 24-bit encoded audio
* **Option B** for PCM 16-bit encoded audio

### NIC setup

TBD

### Receiver side setup

1. Start media_proxy
   ```bash
   sudo media_proxy -d 0000:32:11.1 -i 192.168.96.2 -t 8002
   ```
2. Start FFmpeg to receive packets from MCM and store on the disk
   
   **Option A – PCM 24-bit audio**
   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -re -f mcm_audio \
      -channels 2 \
      -sample_rate 48000 \
      -ptime 1ms \
      -pcm_fmt pcm24 \
      -protocol_type auto \
      -payload_type st30 \
      -ip_addr 192.168.96.1 \
      -port 9001 \
      -i - output.wav
   ```

   **Option B – PCM 16-bit audio**
   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -re -f mcm_audio \
      -channels 2 \
      -sample_rate 48000 \
      -ptime 1ms \
      -pcm_fmt pcm16 \
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

   **Option A – PCM 24-bit audio**
   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <audio-file-path> -f mcm_audio \
      -channels 2 \
      -sample_rate 48000 \
      -ptime 1ms \
      -protocol_type auto \
      -payload_type st30 \
      -ip_addr 192.168.96.2 \
      -port 9001 -
   ```

   **Option B – PCM 16-bit audio**
   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <audio-file-path> -f mcm_audio_pcm16 \
      -channels 2 \
      -sample_rate 48000 \
      -ptime 1ms \
      -protocol_type auto \
      -payload_type st30 \
      -ip_addr 192.168.96.2 \
      -port 9001 -
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
