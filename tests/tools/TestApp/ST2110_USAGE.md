# SMPTE ST 2110 Test Configuration Examples

## ST 2110-20 (Uncompressed Video) Configuration

### Transmitter Example
```bash
# ST 2110-20 uncompressed video transmission
./TxST2110App --transport st2110-20 \
              --ip 224.0.0.1 \
              --port 9002 \
              --payload-type 112 \
              --transport-fmt yuv422p10rfc4175 \
              --width 1920 \
              --height 1080 \
              --fps 60.0 \
              --pixel-fmt yuv422p10le \
              --rdma-provider tcp \
              --rdma-endpoints 1 \
              test_video.yuv
```

### Receiver Example
```bash
# ST 2110-20 uncompressed video reception
./RxST2110App --transport st2110-20 \
              --ip 224.0.0.1 \
              --port 9002 \
              --payload-type 112 \
              --transport-fmt yuv422p10rfc4175 \
              --width 1920 \
              --height 1080 \
              --fps 60.0 \
              --pixel-fmt yuv422p10le \
              --rdma-provider tcp \
              --rdma-endpoints 1 \
              --output received_video.yuv \
              --frames 100
```

## ST 2110-22 (Compressed Video) Configuration

### Transmitter Example
```bash
# ST 2110-22 compressed video (JPEG XS) transmission
./TxST2110App --transport st2110-22 \
              --ip 224.0.0.2 \
              --port 9003 \
              --payload-type 113 \
              --width 3840 \
              --height 2160 \
              --fps 30.0 \
              --pixel-fmt yuv422p10le \
              --rdma-provider verbs \
              --rdma-endpoints 4 \
              test_4k_video.yuv
```

### Receiver Example
```bash
# ST 2110-22 compressed video reception
./RxST2110App --transport st2110-22 \
              --ip 224.0.0.2 \
              --port 9003 \
              --payload-type 113 \
              --width 3840 \
              --height 2160 \
              --fps 30.0 \
              --pixel-fmt yuv422p10le \
              --rdma-provider verbs \
              --rdma-endpoints 4 \
              --output received_4k_video.yuv \
              --frames 50
```

## ST 2110-30 (Uncompressed Audio) Configuration

### Transmitter Example
```bash
# ST 2110-30 uncompressed audio transmission
./TxST2110App --transport st2110-30 \
              --ip 224.0.0.3 \
              --port 9004 \
              --payload-type 111 \
              --channels 8 \
              --sample-rate 48000 \
              --audio-fmt pcm_s24be \
              --packet-time 1ms \
              --rdma-provider tcp \
              --rdma-endpoints 2 \
              test_audio.wav
```

### Receiver Example
```bash
# ST 2110-30 uncompressed audio reception
./RxST2110App --transport st2110-30 \
              --ip 224.0.0.3 \
              --port 9004 \
              --payload-type 111 \
              --channels 8 \
              --sample-rate 48000 \
              --audio-fmt pcm_s24be \
              --packet-time 1ms \
              --rdma-provider tcp \
              --rdma-endpoints 2 \
              --output received_audio.wav \
              --frames 1000
```

## Advanced Configuration Examples

### High-Performance RDMA Configuration
```bash
# Using RDMA verbs with multiple endpoints for low latency
./TxST2110App --transport st2110-20 \
              --ip 224.0.0.1 \
              --port 9002 \
              --rdma-provider verbs \
              --rdma-endpoints 8 \
              --queue-capacity 32 \
              --width 1920 \
              --height 1080 \
              --fps 120.0 \
              test_hfr_video.yuv
```

### Multicast Source Filtering
```bash
# Specify multicast source IP for IGMPv3 source filtering
./RxST2110App --transport st2110-20 \
              --ip 224.0.0.1 \
              --port 9002 \
              --src-ip 192.168.95.10 \
              --payload-type 112 \
              --output filtered_video.yuv
```

### Custom Buffer Configuration
```bash
# Adjust buffer queue capacity and connection delay
./TxST2110App --transport st2110-30 \
              --queue-capacity 64 \
              --delay 100 \
              --channels 2 \
              --sample-rate 96000 \
              test_hires_audio.wav
```

## Supported Formats and Parameters

### Video Pixel Formats
- yuv422p10le (default for most applications)
- yuv422p10rfc4175 (transport format for ST 2110-20)
- yuv420p
- rgb24
- rgba

### Audio Formats
- pcm_s24be (default for ST 2110-30)
- pcm_s16be
- pcm_s32be

### RDMA Providers
- tcp (default, works on all systems)
- verbs (requires RDMA-capable hardware)

### Frame Rates (Video)
- 23.98, 24, 25, 29.97, 30, 50, 59.94, 60, 100, 120 fps

### Sample Rates (Audio)
- 48000 Hz (default)
- 44100 Hz
- 96000 Hz

### Packet Time (Audio)
- 1ms (default)
- 125us
- 250us
- 333us
- 4ms
