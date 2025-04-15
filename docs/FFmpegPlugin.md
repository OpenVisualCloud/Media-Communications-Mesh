# FFmpeg Plugin — Media Communications Mesh

FFmpeg plugin is the software extension of [FFmpeg](https://ffmpeg.org),
a free and open-source software project consisting of a suite of libraries and programs
for handling video, audio, and other multimedia files and streams.

The plugin adds video and audio muxers and demuxers that support sending and receiving
media streams via Media Communications Mesh. It uses SDK API to establish and
manage connections to the Mesh.

## Build and install steps

1. Install Media Communications Mesh. See [Setup Guide](SetupGuide.md) for detailed process.

1. Clone the FFmpeg repository and apply patches.

    By default, FFmpeg Release 7.0 is installed. For FFmpeg Release 6.1,
replace `7.0` with `6.1` in the following script.

   ```bash
   ./clone-and-patch-ffmpeg.sh
   ```
1. Run the FFmpeg configuration tool

   ```bash
   ./configure-ffmpeg.sh
   ```

1. Build and install FFmpeg with the Media Communications Mesh FFmpeg plugin

   ```bash
   ./build-ffmpeg.sh
   ```

Executable file name: `ffmpeg`

FFmpeg with the plugin should be running in privileged mode.

## Muxers and Demuxers

* **Video stream Muxer/Demuxer** `-f mcm`
   * Command line arguments
      * [General parameters](#general-parameters)
      * [SMPTE ST 2110 connection parameters](#smpte-st-2110-connection-parameters--conn_type-st2110)
      * [Multipoint Group connection parameters](#multipoint-group-connection-parameters--conn_type-multipoint-group)
      * [Video payload parameters](#video-payload-parameters)

* **Audio stream Muxer/Demuxer** `-f mcm_audio_pcm16`
   * Audio format PCM 16-bit
   * Command line arguments
      * [General parameters](#general-parameters)
      * [SMPTE ST 2110 connection parameters](#smpte-st-2110-connection-parameters--conn_type-st2110)
      * [Multipoint Group connection parameters](#multipoint-group-connection-parameters--conn_type-multipoint-group)
      * [Audio payload parameters](#audio-payload-parameters)

* **Audio stream Muxer/Demuxer** `-f mcm_audio_pcm24`
   * Audio format PCM 24-bit
   * Command line arguments
      * [General parameters](#general-parameters)
      * [SMPTE ST 2110 connection parameters](#smpte-st-2110-connection-parameters--conn_type-st2110)
      * [Multipoint Group connection parameters](#multipoint-group-connection-parameters--conn_type-multipoint-group)
      * [Audio payload parameters](#audio-payload-parameters)

## Media Proxy SDK API address configuration

Refer to the [SDK API Definition](SDK_API_Definition.md) for the options of configuring the Media Proxy SDK API address.


## Command line arguments

### General parameters

| Argument         | Description                                                                             | Example             |
|------------------| ----------------------------------------------------------------------------------------|---------------------|
| `-conn_type`     | Connection type, `"st2110"` or `"multipoint-group"`. Default "multipoint-group".        | `-conn_type st2110` |
| `-conn_delay`    | Connection creation delay in milliseconds, 0..10000. Default 0.                         | `-conn_delay 100`   |
| `-buf_queue_cap` | Buffer queue capacity, 2, 4, 8, 16, 32, 64, or 128. Default: 8 for video, 16 for audio. | `-buf_queue_cap 64` |

### SMPTE ST 2110 connection parameters (`-conn_type st2110`)

| Argument                  | Description                                                                                                                                 | Example                                    |
|---------------------------|---------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------|
| `-ip_addr`                | **Tx connection**: destination IP address.<br>**Rx connection**: multicast IP address or unicast source IP address. Default "192.168.96.1". | `-ip_addr 224.0.0.1`                       |
| `-port`                   | **Tx connection**: destination port number.<br>**Rx connection**: local port number.                                                        | `-port 9001`                               |
| `-mcast_sip_addr`         | **Rx connection only**: optional multicast source filter IP address.                                                                        | — |
| `-transport`              | **Only for Video Muxer/Demuxer**: SMPTE ST 2110-2x transport type, `"st2110-20"` or `"st2110-22"`. Default "st2110-20".<br>**In Audio Muxers/Demuxers** this argument is not available. The SMPTE ST 2110 transport type is set internally to "st2110-30". | `-transport st2110-20`                     |
| `-payload_type`           | SMPTE ST 2110 payload type, typically between 96..127.<br>Default 112 for video, 111 for audio.                                             | `-payload_type 112`                        |
| `-transport_pixel_format` | Required only for the `"st2110-20"` transport type, default "yuv422p10rfc4175"                                                              | `-transport_pixel_format yuv422p10rfc4175` |

### Multipoint Group connection parameters (`-conn_type multipoint-group`)

| Argument | Description                                                            | Example             |
|----------|------------------------------------------------------------------------|---------------------|
| `-urn`   | Multipoint group Uniform Resource Name, or URN. Default "192.168.97.1" | `-urn 192.168.97.1` |

### Video payload parameters

| Argument        | Description                                                                                    | Example                     |
|-----------------|------------------------------------------------------------------------------------------------|-----------------------------|
| `-video_size`   | Video frame size, `"640x480"`, `"hd720"`, etc. Default "1920x1080".                            | `-video_size 1920x1080`     |
| `-pixel_format` | Video pixel format, `"yuv422p10le"`, `"v210"`, or `"yuv422p10rfc4175"`. Default "yuv422p10le". | `-pixel_format yuv422p10le` |
| `-frame_rate`   | Video frame rate, 25, 50, 59.94, 60, etc. Default 25.                                          | `-frame_rate 60`            |

### Audio payload parameters

| Argument       | Description                                                     | Example              |
|----------------|-----------------------------------------------------------------|----------------------|
| `-channels`    | Number of audio channels, 1, 2, etc. Default: 2.                | `-channels 2`        |
| `-sample_rate` | Audio sample rate, 48000 or 96000. Default 48000.               | `-sample_rate 48000` |
| `-ptime`       | Audio packet time according to SMPTE ST 2110-30. Default "1ms". | `-ptime 1ms`         |


## Example – Run video transmission

This example demonstrates sending a video file from the 1st FFmpeg instance to the 2nd FFmpeg instance via Media Communications Mesh, then streaming it to a remote machine via UDP.

### Run Mesh Agent
   ```bash
   mesh-agent
   ```

### Receiver side setup

1. Start Media Proxy

   ```bash
   sudo media_proxy        \
        -d 0000:32:01.1    \
        -i 192.168.96.11   \
        -r 192.168.97.11   \
        -p 9200-9299       \
        -t 8002
   ```

1. Start FFmpeg to receive frames from Media Communications Mesh and stream them to a remote machine via UDP

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -f mcm                      \
      -conn_type st2110                                              \
      -transport st2110-20                                           \
      -ip_addr 192.168.96.10                                         \
      -port 9001                                                     \
      -frame_rate 60                                                 \
      -video_size 1920x1080                                          \
      -pixel_format yuv422p10le                                      \
      -i - -vcodec mpeg4 -f mpegts udp://<remote-ip>:<remote-port>
   ```

### Sender side setup

1. Start Media Proxy

   ```bash
   sudo media_proxy        \
        -d 0000:32:01.0    \
        -i 192.168.96.10   \
        -r 192.168.97.10   \
        -p 9100-9199       \
        -t 8001
   ```

2. Start FFmpeg to stream a video file to the receiver via Media Communications Mesh

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <video-file-path> -f mcm   \
      -conn_type st2110                                                \
      -transport st2110-20                                             \
      -ip_addr 192.168.96.11                                           \
      -port 9001                                                       \
      -frame_rate 60                                                   \
      -video_size 1920x1080                                            \
      -pixel_format yuv422p10le -
   ```

   When sending a raw video file that lack metadata, you must explicitly provide FFmpeg
   with the necessary video frame details. This includes specifying the format
   `-f rawvideo`, pixel format `-pix_fmt`, and resolution `-s WxH`. Example:

   ```bash
   ffmpeg -f rawvideo -pix_fmt yuv422p10le -s 1920x1080 -i <video-file-path> ...
   ```

### VLC player setup

On the remote machine start the VLC player and open a network stream from the following URL:
```text
udp://@:1234
```


## Example – Run audio transmission, PCM 24-bit

This example demonstrates sending a PCM 24-bit encoded audio file from the 1st FFmpeg instance to the 2nd FFmpeg instance via Media Communications Mesh.

### Run Mesh Agent
   ```bash
   mesh-agent
   ```

### Receiver side setup

1. Start Media Proxy

   ```bash
   sudo media_proxy        \
        -d 0000:32:01.1    \
        -i 192.168.96.11   \
        -r 192.168.97.11   \
        -p 9200-9299       \
        -t 8002
   ```

2. Start FFmpeg to receive packets from Media Communications Mesh and store on the disk

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -f mcm_audio_pcm24   \
      -conn_type st2110                                       \
      -ip_addr 192.168.96.10                                  \
      -port 9001                                              \
      -channels 2                                             \
      -sample_rate 48000                                      \
      -ptime 1ms                                              \
      -i - output.wav
   ```

### Sender side setup

1. Start Media Proxy

   ```bash
   sudo media_proxy        \
        -d 0000:32:01.0    \
        -i 192.168.96.10   \
        -r 192.168.97.10   \
        -p 9100-9199       \
        -t 8001
   ```

2. Start FFmpeg to stream an audio file to the receiver via Media Communications Mesh

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <audio-file-path> -f mcm_audio_pcm24   \
      -conn_type st2110                                                            \
      -ip_addr 192.168.96.11                                                       \
      -port 9001                                                                   \
      -channels 2                                                                  \
      -sample_rate 48000                                                           \
      -ptime 1ms -
   ```

## Troubleshooting

### Shared libraries error
While the FFmpeg plugin build was successful, an attempt to run `ffmpeg` results in the shared libraries error:
```text
root@my-machine:~# ffmpeg

ffmpeg: error while loading shared libraries: libavfilter.so.9: cannot open shared object file: No such file or directory
```

**Resolution:**
Export the `LD_LIBRARY_PATH` directory path in your shell session by pointing to `/usr/local/lib`. Examples:
```text
root@my-machine:~# LD_LIBRARY_PATH=/usr/local/lib ffmpeg

ffmpeg version n6.1.1-152-ge821e6c21d Copyright (c) 2000-2023 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.4.0-1ubuntu1~22.04)
```
Using export:
```text
root@my-machine:~# export LD_LIBRARY_PATH=/usr/local/lib
root@my-machine:~# ffmpeg

ffmpeg version n6.1.1-152-ge821e6c21d Copyright (c) 2000-2023 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.4.0-1ubuntu1~22.04)
```

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
