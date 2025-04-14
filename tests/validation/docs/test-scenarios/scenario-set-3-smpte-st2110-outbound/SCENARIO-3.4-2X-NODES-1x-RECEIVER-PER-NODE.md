# Scenario Set 3 – SMPTE ST 2110 Outbound Transmission

## Scenario 3.4 – 2x Nodes / 1x Receiver per Node

### Configuration

```mermaid
flowchart LR
    subgraph Node B
        rxB1((Rx App 2))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        tx((Tx App))
        rxA1((Rx App 1))
        proxy1(Media Proxy A)
    end
    sw1(["Network
         Switch"])
    sw2(["Network
         Switch"])
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 -- ST 2110 ----> sw1
    proxy1 -- RDMA ----> sw2
    sw2 -- RDMA --> proxy2
    proxy2 --> rxB1
    sw1 -- ST 2110 --> dev
```

### Payload Options

* Video – SMPTE ST 2110-20 Uncompressed Video
* Video – SMPTE ST 2110-22 Compressed Video (JPEG XS)
* Audio – SMPTE ST 2110-30 Uncompressed Audio (PCM)

### Notes

1. For SMPTE ST 2110-22 Compressed Video, consider the following
    * Tx App transmits uncompressed video frames.
    * Rx App 1, 2 receive uncompressed video frames.
    * External SMPTE ST 2110 compliant input device receives compressed video.
