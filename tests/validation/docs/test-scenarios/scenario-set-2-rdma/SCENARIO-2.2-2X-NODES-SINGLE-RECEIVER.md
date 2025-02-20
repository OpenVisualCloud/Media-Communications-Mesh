# Scenario Set 2 – RDMA Transmission

## Scenario 2.2 – 2x Nodes / Single Receiver

### Configuration

```mermaid
flowchart LR
    subgraph Node A
        tx((Tx App))
        proxy1(Media Proxy A)
    end
    subgraph Node B
        rxB1((Rx App))
        proxy2(Media Proxy B)
    end
    sw(["Network
        Switch"])
    tx --> proxy1
    proxy1 -- RDMA --> sw
    sw -- RDMA --> proxy2
    proxy2 --> rxB1
```

### Payload Options

* Blob
* Video – Uncompressed
* Audio

#### Supported Blob  (random binary block) formats

100 MB data generated based on /dev/random

#### Supported video formats

Resolutions:

* Full HD 1920x1080
* 4k 3840x2160

Framerate (FPS):
* 59.94
* 60

Color format:
* YUV 4:2:2 10-bit planar

Interlace: 
* progressive

Packing:
* gpm
* bpm
* gpm_sl

Pacing:
* wide
* narrow
* linear 


#### Supported audio formats

Audio formats:
* PCM 8-bit Big-Endian
* PCM 16-bit Big-Endian
* PCM 24-bit Big-Endian

Sample Rates:
* 44100 kHz
* 48000 kHz
* 96000 kHz

Packet Time (Ptime):
* 1 ms (48/96 samples per group)

Number of channels:
* 1 (M - Mono)
* 2 (ST - Stereo)

Sample Depth:
* pcm_s24be

Test mode:
* frame

### Transmission Modes

* standalone
* FFmpeg


### Tested parameters
buffer size

metadata size

number of buffers in allocated queue.

Additionally:

Checking fps

Checking number of frames obtained
( Our current implementation of RxTxApp may encounter issues supporting this. )

### Test Cases
