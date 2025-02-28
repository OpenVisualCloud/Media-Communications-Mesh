> **Note:** This document provides generic information for all scenarios in the `/Media-Communications-Mesh/tests/validation/docs/test-scenarios/` directory.

# Transmission Modes

* standalone
* FFmpeg

# Payload Options

* Blob (random binary block)
* Video – Uncompressed
* Audio

## Tested Blob (random binary block) formats

100 MB data generated based on /dev/random

```bash
dd if=/dev/random of=random_data.bin bs=1M count=100
```

## Tested video formats

| Parameter       | Standalone          | FFmpeg                |
|-----------------|----------------------|------------------------|
| Resolutions     | Full HD 1920x1080    | Full HD 1920x1080      |
|                 | 4k 3840x2160         | 4k 3840x2160           |
| Framerate (FPS) | 59.94                | 59.94                  |
|                 | 60                   | 60                     |
| Color format    | YUV 4:2:2 10-bit planar | YUV 4:2:2 10-bit planar |
| Interlace       | progressive          | progressive            |
| Packing         | gpm                  | can't be changed in ffmpeg |
|                 | bpm                  |          -             |
|                 | gpm_sl               |          -             |
| Pacing          | wide                 | can't be changed in ffmpeg |
|                 | narrow               |          -             |
|                 | linear               |          -             |

To check file data using FFmpeg, you can use the following command template:

```bash
ffmpeg -f rawvideo -pixel_format <pixel_format> -video_size <width>x<height> -i <input_file> -vframes <number_of_frames> -f null -
```

## Tested audio formats

| Parameter         | Standalone          | FFmpeg                |
|-------------------|---------------------|-----------------------|
| Audio formats     | PCM 8-bit Big-Endian| -                     |
|                   | PCM 16-bit Big-Endian| PCM 16-bit Big-Endian |
|                   | PCM 24-bit Big-Endian| PCM 24-bit Big-Endian |
| Sample Rates      | 44100 Hz            | -                     |
|                   | 48000 Hz            | 48000 Hz              |
|                   | 96000 Hz            | 96000 Hz              |
| Packet Time*      | 1 ms (48/96 samples per group) | 1 ms (48/96 samples per group) |
| Number of channels| 1 (M - Mono)        | 1 (M - Mono)          |
|                   | 2 (ST - Stereo)     | 2 (ST - Stereo)       |
| Sample Depth      | pcm_s24be           | pcm_s24be             |
| Test mode         | frame               | frame                 |

*For some sample rates like 44100 Hz, 1 ms is not available due to the fact that 1 ms at a sample rate of 44100 Hz would require 44.1 samples (since 44100 samples/second * 0.001 seconds = 44.1 samples), which is not possible. Consider adjusting the packet time (1.09ms for 44100 Hz) or using a compatible sample rate like 48000 Hz for 1 ms packet time compatibility.

# Tested parameters

* buffer size
* metadata size
* number of buffers in allocated queue.

Additionally:
* Checking fps
* Checking number of frames obtained

# Test Run Steps

1. Start one `mesh-agent` per cluster.
2. Start `media_proxies` for the transmitter and receiver. Provide the IP of the node with the `mesh-agent` and the port number (found in `mesh-agent` logs) to the ones not running on the node with the `mesh-agent`.
3. Start the receivers.
4. Start the transmitter.

This sequence ensures that the transmitter is ready to receive the transmission.