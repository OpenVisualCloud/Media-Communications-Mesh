# MCM Data Plane SDK

[![Ubuntu Build](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml/badge.svg)](https://github.com/OpenVisualCloud/Media-Communications-Mesh/actions/workflows/ubuntu-build.yml)
[![BSD 3-Clause][license-img]][license]

## Overview

The MCM SDK is a lightweight and versatile library designed to facilitate seamless media data transportation between microservices, with or without the MCM Media Proxy. By leveraging the MCM SDK, developers can easily establish efficient media streaming and enable zero-copied handling of media data transfers.

## Installation

1. Dependencies

Install the required dependencies by running the command:

```bash
$ sudo apt-get install -y cmake libbsd-dev
```

2. Build SDK Library

```bash
$ cmake -B out .
$ cmake --build out -j 4
```

3. Install on System

To install the MCM SDK on your system, execute the following command:

```bash
$ cmake --install out
```

## Sample Applications

The usage of SDK APIs could refer to the "samples" applications.

1. Sender

Sample code for the application which send out data to others.

Source code: samples/sender_app.c
```bash
Usage: sender_app [OPTION]
-H, --help                              Print this help and exit
-r, --rcv_ip=<ip_address>               Receiver's IP address (default: 127.0.0.1)
-i, --rcv_port=<port_number>            Receiver's port number (default: 9001)
-s, --send_ip=<ip_address>              Sender's IP address (default: 127.0.0.1)
-p, --send_port=<port_number>           Sender's port number (default: 9001)
-o, --protocol=<protocol_type>          Set protocol type (default: auto)
-t, --type=<payload_type>               Payload type (default: st20)
-k, --socketpath=<socket_path>          Set memif socket path (default: /run/mcm/mcm_rx_memif.sock)
-m, --master=<is_master>                Set memif conn is master (default: 1 for sender, 0 for recver)
-d, --interfaceid=<interface_id>        Set memif conn interface id (default: 0)
-b, --file=<input_file>                 Input file name (optional)
-l, --loop=<is_loop>                    Set infinite loop sending (default: 0)
--------------------------------------   VIDEO (ST2x)   --------------------------------------
-w, --width=<frame_width>               Width of test video frame (default: 1920)
-h, --height=<frame_height>             Height of test video frame (default: 1080)
-f, --fps=<video_fps>                   Test video FPS (frame per second) (default: 30.00)
-x, --pix_fmt=<pixel_format>            Pixel format (default: yuv422p10le)
-n, --number=<number_of_frames>         Total frame number to send (default = inf: 0)
--------------------------------------   AUDIO (ST3x)   --------------------------------------
-a, --audio_type=<audio_type>           Define audio type [frame|rtp] (default: frame)
-j, --audio_format=<audio_format>       Define audio format [pcm8|pcm16|pcm24|am824] (default: pcm16)
-g, --audio_sampling=<audio_sampling>   Define audio sampling [48k|96k|44k] (default: 48k)
-e, --audio_ptime=<audio_ptime>         Define audio ptime [1ms|125us|250us|333us|4ms|80us|1.09ms|0.14ms|0.09ms] (default: 1ms)
-c, --audio_channels=<channels>         Define number of audio channels [1|2] (default: 1)
-------------------------------------- ANCILLARY (ST4x) --------------------------------------
-q, --anc_type=<anc_type>               Define anc type [frame|rtp] (default: frame)
```

2. Receiver

Sample code for the application which receive data from others.

Source code: samples/sender_app.c
```bash
$ ./build/samples/recver_app
Usage: recver_app [OPTION]
-H, --help                              Print this help and exit
-r, --rcv_ip=<ip_address>               Receiver's IP address (default: 127.0.0.1)
-i, --rcv_port=<port_number>            Receiver's port number (default: 9001)
-s, --send_ip=<ip_address>              Sender's IP address (default: 127.0.0.1)
-p, --send_port=<port_number>           Sender's port number (default: 9001)
-o, --protocol=<protocol_type>          Set protocol type (default: auto)
-t, --type=<payload_type>               Payload type (default: st20)
-k, --socketpath=<socket_path>          Set memif socket path (default: /run/mcm/mcm_rx_memif.sock)
-m, --master=<is_master>                Set memif conn is master (default: 1 for sender, 0 for recver)
-d, --interfaceid=<interface_id>        Set memif conn interface id (default: 0)
-b, --dumpfile=<file_name>              Save stream to local file (example: data-sdk.264)
--------------------------------------   VIDEO (ST2x)   --------------------------------------
-w, --width=<frame_width>               Width of test video frame (default: 1920)
-h, --height=<frame_height>             Height of test video frame (default: 1080)
-f, --fps=<video_fps>                   Test video FPS (frame per second) (default: 30.00)
-x, --pix_fmt=<pixel_format>            Pixel format (default: yuv422p10le)
--------------------------------------   AUDIO (ST3x)   --------------------------------------
-a, --audio_type=<audio_type>           Define audio type [frame|rtp] (default: frame)
-j, --audio_format=<audio_format>       Define audio format [pcm8|pcm16|pcm24|am824] (default: pcm16)
-g, --audio_sampling=<audio_sampling>   Define audio sampling [48k|96k|44k] (default: 48k)
-e, --audio_ptime=<audio_ptime>         Define audio ptime [1ms|125us|250us|333us|4ms|80us|1.09ms|0.14ms|0.09ms] (default: 1ms)
-c, --audio_channels=<channels>         Define number of audio channels [1|2] (default: 1)
-------------------------------------- ANCILLARY (ST4x) --------------------------------------
-q, --anc_type=<anc_type>               Define anc type [frame|rtp] (default: frame)
```

3. RAISR Microservice

Standalone microservice to apply "super resolution" to RAW YUV format video frames.
This application use MCM DP SDK to handle on the data input/output functions.

Source code: (will be released later)

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
