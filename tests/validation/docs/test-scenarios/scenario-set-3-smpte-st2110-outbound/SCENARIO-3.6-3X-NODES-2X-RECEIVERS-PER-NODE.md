# Scenario Set 3 – SMPTE ST 2110 Outbound Transmission

## Scenario 3.6 – 3x Nodes / 2x Receivers per Node

### Configuration

```mermaid
flowchart LR
    subgraph Node C
        rxC1((Rx App 5))
        rxC2((Rx App 6))
        proxy3(Media Proxy C)
    end
    subgraph Node B
        rxB1((Rx App 3))
        rxB2((Rx App 4))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        tx((Tx App))
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
    end
    sw1(["Network
          Switch"])
    sw2(["Network
          Switch"])
    sw3(["Network
          Switch"])
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- ST 2110 ----> sw1
    proxy1 -- RDMA ----> sw2
    sw2 -- RDMA --> proxy2
    proxy1 -- RDMA ----> sw3
    sw3 -- RDMA --> proxy3
    proxy2 --> rxB1
    proxy2 --> rxB2
    proxy3 --> rxC1
    proxy3 --> rxC2
    sw1 -- ST 2110 --> dev
```

### Payload Options

* Video – SMPTE ST 2110-20 Uncompressed Video
* Video – SMPTE ST 2110-22 Compressed Video (JPEG XS)
* Audio – SMPTE ST 2110-30 Uncompressed Audio (PCM)

### Notes

1. For SMPTE ST 2110-22 Compressed Video, consider the following
    * Tx App transmits uncompressed video frames.
    * Rx App 1, 2, 3, 4, 5, 6 receive uncompressed video frames.
    * External SMPTE ST 2110 compliant input device receives compressed video.
