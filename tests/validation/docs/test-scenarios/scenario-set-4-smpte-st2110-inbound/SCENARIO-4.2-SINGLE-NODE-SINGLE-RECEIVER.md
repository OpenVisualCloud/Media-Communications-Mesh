# Scenario Set 4 – SMPTE ST 2110 Inbound Transmission

## Scenario 4.2 – Single Node / Single Receiver

### Configuration

```mermaid
flowchart LR
    subgraph Single Node
        rx((Rx App))
        proxy(Media Proxy)
    end
    dev("ST 2110 Compliant
         Output Device
         _(Camera)_")
    sw(["Network
        Switch"])
    dev -- ST 2110 --> sw
    sw -- ST 2110 --> proxy
    proxy --> rx
```

### Output Device – Destination IP Address Options

* Unicast IP address
* Multicast IP address

### Payload Options

* Video – SMPTE ST 2110-20 Uncompressed Video
* Video – SMPTE ST 2110-22 Compressed Video (JPEG XS)
* Audio – SMPTE ST 2110-30 Uncompressed Audio (PCM)

### Notes

1. For SMPTE ST 2110-22 Compressed Video, consider the following
    * External SMPTE ST 2110 compliant output device transmits compressed video.
    * Rx App receives uncompressed video frames.
