> **Note:** This document provides generic information for all scenarios in the `/Media-Communications-Mesh/tests/validation/docs/test-scenarios/` directory. Provided test cases may not cover all supported parameters.

# Transmission Modes

* standalone - with user application built on API
* FFmpeg - using FFmpeg plugin


# Payload Options

* Blob (random binary block)
* Video – uncompressed (raw)
* Audio


## Blob (random binary block) format

At least 100 MB data generated based on `/dev/random`.

Generation command example using `dd` tool:
```bash
dd if=/dev/random of=random_data.bin bs=1M count=100
```


## Uncompressed video formats

| Parameter       | Standalone          | FFmpeg                |
|-----------------|----------------------|------------------------|
| Resolutions     | 1920x1080 (Full HD)  | 1920x1080 (Full HD)    |
|                 | 3840x2160 (4k)       | 3840x2160 (4k)         |
| Framerate (FPS) | 59.94                | 59.94                  |
|                 | 60                   | 60                     |
| Color format    | YUV 4:2:2 10-bit planar | YUV 4:2:2 10-bit planar |
| Interlace       | progressive          | progressive            |
| Packing         | gpm                  | *cannot be changed in FFmpeg plugin* |
|                 | bpm                  | bpm (default)          |
|                 | gpm_sl               | *cannot be changed in FFmpeg plugin* |
| Pacing          | wide                 | **cannot be changed in FFmpeg plugin**? |
|                 | narrow               | **cannot be changed in FFmpeg plugin**? |
|                 | linear               | **cannot be changed in FFmpeg plugin**? |

<!--TODO: Is bpm the only packing format that can be used by FFmpeg plugin? -->
<!--TODO: Which pacing is used by FFmpeg plugin by default? -->

To check file data using FFmpeg, you can use the following command template:

```bash
ffmpeg -f rawvideo -pixel_format <pixel_format> -video_size <width>x<height> -i <input_file> -vframes <number_of_frames> -f null -
```


### Information about video transmission parameters

Parameters provided in brackets refer to SDP (Session Description Protocol) values.

Packing mode abbreviations:
* gpm - General Packing Mode (`PM=2110GPM`)
* bpm - Block Packing Mode (`PM=2110BPM`)
* gpm_sl - General Packing Mode (with single scan line, `PM=2110???`)

Pacing:
* wide (`TP=2110TPW`)
* narrow (`TP=2110TPN`)
* linear - narrow linear (`TP=2110TPNL`)

This section is partially based on [*VSF TR-05 Version 1.0*](https://static.vsf.tv/download/technical_recommendations/VSF_TR-05_2018-06-23.pdf) ([CC-BY-ND](https://creativecommons.org/licenses/by-nd/4.0/)), accessed on 2025-03-03.


## Audio formats

| Parameter          | Standalone          | FFmpeg                |
|--------------------|---------------------|-----------------------|
| Audio sample depth | PCM 8-bit Big-Endian (`pcm_s8be`)   | *not available*                     |
|                    | PCM 16-bit Big-Endian (`pcm_s16be`) | PCM 16-bit Big-Endian (`pcm_s16be`) |
|                    | PCM 24-bit Big-Endian (`pcm_s24be`) | PCM 24-bit Big-Endian (`pcm_s24be`) |
| Sample rate        | 44100 Hz*                      | *not available*       |
|                    | 48000 Hz                       | 48000 Hz              |
|                    | 96000 Hz                       | 96000 Hz              |
| Packet time        | 1 ms/1.09 ms* (48/96 samples per group) | 1 ms (48/96 samples per group) |
| Number of channels | 1 (M - Mono)        | 1 (M - Mono)          |
|                    | 2 (ST - Stereo)     | 2 (ST - Stereo)       |
| Test mode          | frame               | frame                 |

*1 ms Packet Time is not available for 44100 Hz sample rate. It would require a non-natural number of samples (since 44100 samples/second * 0.001 seconds = 44.1 samples), which is not possible. Adjust the packet time to 1.09ms for 44100 Hz (48 samples per packet).


# Tested parameters

* buffer size
* metadata size
* number of buffers in allocated queue.

Additional checks:
* if transmitted frames per second (fps) are as was setup
* if received fps keep up with sent fps
* if number of frames obtained closely matches the number of frames sent


# Test execution

1. Start one `mesh-agent` per cluster.
2. Start `media_proxies` for the transmitter and receiver (always only a single instance per node). Provide the IP of the node with the `mesh-agent` and the port number (found in `mesh-agent` logs) to the ones not running on the node with the `mesh-agent`.
3. Start the receivers.
4. Start the transmitter.

Above sequence ensures that the receiver is ready to receive the data as soon as possible. Even then, there is a connection initialization delay, resulting in a few bytes (B) missing at the beginning of the transmission. Such behavior should be considered normal, except when that delay is longer than usual (more than a few frames).


# JT-NM 2022 compliance

Media transmitted with ST 2110 are received on a receiving system with a `tcpdump` session dumping to a `*.pcap` file. Such session is performed separately from the main tests to avoid performance issues. The `*.pcap` file is later uploaded to an [EBU LIST tool](https://github.com/ebu/pi-list/) to check the compliance with JT-NM 2022.

For more information, check [Joint Task Force on Networked Media](https://www.jt-nm.org/jt-nm-tested) website.
