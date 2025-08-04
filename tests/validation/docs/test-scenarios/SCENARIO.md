# Test Scenario Documentation

> **Note:** This document provides generic information for all scenarios in the `/Media-Communications-Mesh/tests/validation/docs/test-scenarios/` directory. Provided test cases may not cover all supported parameters.

## Transmission Modes

* standalone - with user application built on API
* FFmpeg - using FFmpeg plugin


## Payload Options

* Blob (random binary block)
* Video – uncompressed (raw)
* Audio


## Blob (random binary block) format

At least 100 MB data generated based on `/dev/random`.

Generation command example using `dd` tool:
```bash
dd if=/dev/random of=random_data.bin bs=1 count=100000000
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
| Packing         | gpm                  | *cannot be set in FFmpeg plugin* |
|                 | bpm                  | *cannot be set in FFmpeg plugin* |
|                 | gpm_sl               | *cannot be set in FFmpeg plugin* |
| Pacing          | wide                 | *cannot be set in FFmpeg plugin* |
|                 | narrow               | narrow (default in FFmpeg plugin) |
|                 | linear               | *cannot be set in FFmpeg plugin* |

<!-- Note: The FFmpeg plugin uses "narrow" pacing as the default according to the SDK API examples.
     Packing mode is not directly configurable in the FFmpeg plugin as it uses the SDK's default behavior. -->

## Analyzing Media Files

To validate raw video files using FFmpeg:

```bash
ffmpeg -f rawvideo -pixel_format <pixel_format> -video_size <width>x<height> -i <input_file> -vframes <number_of_frames> -f null -
```

To analyze video metadata and content details:

```bash
ffprobe -v verbose -show_format -show_streams -print_format json <input_file>
```

To verify audio file parameters:

```bash
ffprobe -v quiet -print_format json -show_streams -select_streams a:0 <audio_file>
```


### Information about video transmission parameters

Parameters provided in brackets refer to SDP (Session Description Protocol) values.

Packing mode abbreviations (according to SMPTE ST 2110-20):
* gpm - General Packing Mode (`PM=2110GPM`) - Multiple scan lines per packet, potentially more efficient use of bandwidth
* bpm - Block Packing Mode (`PM=2110BPM`) - Packs scan lines in blocks, helps with compatibility
* gpm_sl - General Packing Mode with single scan line - Only one scan line per packet, may reduce latency

Pacing (according to SMPTE ST 2110):
* wide (`TP=2110TPW`) - More permissive traffic shaping, allows bursts within limits
* narrow (`TP=2110TPN`) - Stricter traffic shaping with more uniform packet distribution over time
* linear - narrow linear (`TP=2110TPNL`) - Most strict pacing with completely linear distribution

Note: When using FFmpeg plugin with MCM, the pacing is set to "narrow" by default, as specified in the SDK API examples.

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

*1 ms Packet Time is not available for 44100 Hz sample rate. It would require a non-natural number of samples (since 44100 samples/second *0.001 seconds = 44.1 samples*), which is not possible. Adjust the packet time to 1.09ms for 44100 Hz (48 samples per packet).


## Test Cases

### Video Test Cases

| # | Standalone / FFmpeg | Resolution | Framerate |
|---|---------------------|------------|-----------|
| 1 | Standalone | 1920x1080 (Full HD) | 59.94 (60000/1001) |
| 2 | Standalone | 1920x1080 (Full HD) | 60 |
| 3 | FFmpeg | 1920x1080 (Full HD) | 59.94 (60000/1001) |
| 4 | FFmpeg | 1920x1080 (Full HD) | 60 |
| 5 | Standalone | 3840x2160 (4k) | 59.94 (60000/1001) |
| 6 | Standalone | 3840x2160 (4k) | 60 |
| 7 | FFmpeg | 3840x2160 (4k) | 59.94 (60000/1001) |
| 8 | FFmpeg | 3840x2160 (4k) | 60 |

## Audio Test Cases

| # | Standalone / FFmpeg | Sample depth | Samplerate | Packettime | Channels |
|---|---------------------|--------------|------------|------------|----------|
| 1 | Standalone | PCM 8-bit Big-Endian (pcm_s8be) | 44100 Hz | 1.09 ms | 1 (M - Mono) |
| 2 | Standalone | PCM 8-bit Big-Endian (pcm_s8be) | 44100 Hz | 1.09 ms | 2 (S - Stereo) |
| 3 | Standalone | PCM 16-bit Big-Endian (pcm_s16be) | 44100 Hz | 1.09 ms | 1 (M - Mono) |
| 4 | Standalone | PCM 16-bit Big-Endian (pcm_s16be) | 44100 Hz | 1.09 ms | 2 (S - Stereo) |
| 5 | Standalone | PCM 24-bit Big-Endian (pcm_s24be) | 44100 Hz | 1.09 ms | 1 (M - Mono) |
| 6 | Standalone | PCM 24-bit Big-Endian (pcm_s24be) | 44100 Hz | 1.09 ms | 2 (S - Stereo) |
| 7 | Standalone | PCM 8-bit Big-Endian (pcm_s8be) | 48000 Hz | 1 ms | 1 (M - Mono) |
| 8 | Standalone | PCM 8-bit Big-Endian (pcm_s8be) | 48000 Hz | 1 ms | 2 (S - Stereo) |
| 9 | Standalone | PCM 16-bit Big-Endian (pcm_s16be) | 48000 Hz | 1 ms | 1 (M - Mono) |
| 10 | Standalone | PCM 16-bit Big-Endian (pcm_s16be) | 48000 Hz | 1 ms | 2 (S - Stereo) |
| 11 | Standalone | PCM 24-bit Big-Endian (pcm_s24be) | 48000 Hz | 1 ms | 1 (M - Mono) |
| 12 | Standalone | PCM 24-bit Big-Endian (pcm_s24be) | 48000 Hz | 1 ms | 2 (S - Stereo) |
| 13 | Standalone | PCM 8-bit Big-Endian (pcm_s8be) | 96000 Hz | 1 ms | 1 (M - Mono) |
| 14 | Standalone | PCM 8-bit Big-Endian (pcm_s8be) | 96000 Hz | 1 ms | 2 (S - Stereo) |
| 15 | Standalone | PCM 16-bit Big-Endian (pcm_s16be) | 96000 Hz | 1 ms | 1 (M - Mono) |
| 16 | Standalone | PCM 16-bit Big-Endian (pcm_s16be) | 96000 Hz | 1 ms | 2 (S - Stereo) |
| 17 | Standalone | PCM 24-bit Big-Endian (pcm_s24be) | 96000 Hz | 1 ms | 1 (M - Mono) |
| 18 | Standalone | PCM 24-bit Big-Endian (pcm_s24be) | 96000 Hz | 1 ms | 2 (S - Stereo) |
| 19 | FFmpeg | PCM 16-bit Big-Endian (pcm_s16be) | 48000 Hz | 1 ms | 1 (M - Mono) |
| 20 | FFmpeg | PCM 16-bit Big-Endian (pcm_s16be) | 48000 Hz | 1 ms | 2 (S - Stereo) |
| 21 | FFmpeg | PCM 24-bit Big-Endian (pcm_s24be) | 48000 Hz | 1 ms | 1 (M - Mono) |
| 22 | FFmpeg | PCM 24-bit Big-Endian (pcm_s24be) | 48000 Hz | 1 ms | 2 (S - Stereo) |
| 23 | FFmpeg | PCM 16-bit Big-Endian (pcm_s16be) | 96000 Hz | 1 ms | 1 (M - Mono) |
| 24 | FFmpeg | PCM 16-bit Big-Endian (pcm_s16be) | 96000 Hz | 1 ms | 2 (S - Stereo) |
| 25 | FFmpeg | PCM 24-bit Big-Endian (pcm_s24be) | 96000 Hz | 1 ms | 1 (M - Mono) |
| 26 | FFmpeg | PCM 24-bit Big-Endian (pcm_s24be) | 96000 Hz | 1 ms | 2 (S - Stereo) |

## Tested Parameters and Metrics

## Blob Test Cases

| # | Standalone / FFmpeg | Data size | Packet size |
|---|---------------------|-----------|-------------|
| 1 | Standalone | 100 MB | 100 kB |

## Tested Parameters and Metrics

### Configuration Parameters
* **Buffer Size**: Size of memory buffers allocated for media transmission
* **Metadata Size**: Maximum size of metadata attached to each frame
* **Queue Capacity**: Number of buffers in allocated queue (configurable with `bufferQueueCapacity` parameter)
* **Connection Delay**: Time between connection establishment and first frame transmission (configurable with `connCreationDelayMilliseconds`)

## Performance Metrics
* **Frame Rate Accuracy**: Verifying that transmitted frames per second (fps) match the configured rate
* **Reception Rate**: Confirming that received fps keeps up with sent fps without drops
* **Frame Integrity**: Ensuring the number of frames received closely matches the number of frames sent
* **Latency**: Measuring end-to-end delay between transmission and reception

## Testing Tools
* Built-in logging and statistics from Media Proxy
* FFmpeg stats output (`-stats` option)
* Custom test applications in the `tests/` directory


## Manual Test Execution

1. Start one `mesh-agent` per cluster:
   ```bash
   mesh-agent
   ```

2. Start `media_proxy` for the transmitter and receiver (always only a single instance per node):

Receiver side:
   ```bash
   sudo media_proxy        \
        -d <pci_device>    \
        -i <st2110_ip>     \
        -r <rdma_ip>       \
        -p 9200-9299       \
        -t 8002
   ```

Transmitter side:
   ```bash
   sudo media_proxy        \
        -d <pci_device>    \
        -i <st2110_ip>     \
        -r <rdma_ip>       \
        -p 9100-9199       \
        -t 8001
   ```

3. Start the receivers (e.g., using FFmpeg for video reception):
   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -f mcm                      \
      -conn_type st2110                                              \
      -transport st2110-20                                           \
      -ip_addr <source_ip>                                           \
      -port <port>                                                   \
      -frame_rate <frame_rate>                                       \
      -video_size <width>x<height>                                   \
      -pixel_format <pixel_format>                                   \
      -i - -f <output_format> <output_destination>
   ```

4. Start the transmitter (e.g., using FFmpeg for video transmission):
   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <input_file> -f mcm      \
      -conn_type st2110                                              \
      -transport st2110-20                                           \
      -ip_addr <destination_ip>                                      \
      -port <port>                                                   \
      -frame_rate <frame_rate>                                       \
      -video_size <width>x<height>                                   \
      -pixel_format <pixel_format> -
   ```

This sequence ensures that the receiver is ready to receive the data as soon as possible. Even then, there is a connection initialization delay, resulting in a few packets missing at the beginning of the transmission. Such behavior should be considered normal, except when that delay is longer than usual (more than a few frames).


## JT-NM Compliance Testing

Media transmitted with SMPTE ST 2110 protocols is received on a test system with a `tcpdump` session capturing network traffic to a `*.pcap` file. This compliance capture is performed separately from the main tests to avoid performance issues that might affect the measurements.

To capture network traffic for compliance testing:

```bash
sudo tcpdump -i <interface_name> -w capture.pcap 'udp and port <port_number>'
```

The resulting `*.pcap` file is later uploaded to the [EBU LIST tool](https://github.com/ebu/pi-list/) to verify compliance with JT-NM (Joint Task Force on Networked Media) specifications. The LIST tool analyzes various aspects including:

- Packet timing compliance (according to selected pacing profile)
- RTP header consistency
- Payload format compliance
- Media quality parameters

For more information about JT-NM compliance testing and specifications, visit the [Joint Task Force on Networked Media website](https://www.jt-nm.org/jt-nm-tested).
