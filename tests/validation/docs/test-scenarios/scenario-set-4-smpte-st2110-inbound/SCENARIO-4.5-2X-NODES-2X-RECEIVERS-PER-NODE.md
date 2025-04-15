# Scenario Set 4 – SMPTE ST 2110 Inbound Transmission

## Scenario 4.5 – 2x Nodes / 2x Receivers per Node

### Configuration

```mermaid
flowchart LR
    subgraph Node A
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
    end
    subgraph Node B
        rxB1((Rx App 3))
        rxB2((Rx App 4))
        proxy2(Media Proxy B)
    end
    dev("ST 2110 Compliant
         Output Device
         _(Camera)_")
    sw(["Network
        Switch"])
    dev -- ST 2110 --> sw
    sw -- ST 2110 --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- RDMA ----> proxy2
    proxy2 --> rxB1
    proxy2 --> rxB2
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
    * Rx App 1, 2, 3, 4 receive uncompressed video frames.
