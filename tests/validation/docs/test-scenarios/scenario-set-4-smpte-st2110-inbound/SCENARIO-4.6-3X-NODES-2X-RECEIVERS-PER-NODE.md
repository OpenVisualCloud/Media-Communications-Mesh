# Scenario Set 4 – SMPTE ST 2110 Inbound Transmission

## Scenario 4.6 – 3x Nodes / 2x Receivers per Node

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
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
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
    proxy1 -- RDMA ----> proxy3
    proxy2 --> rxB1
    proxy2 --> rxB2
    proxy3 --> rxC1
    proxy3 --> rxC2
```

### Payload Options

* Video – Uncompressed ST 2110-20
* Video – Compressed ST 2110-22
* Audio – Uncompressed ST 2110-30

### Notes

1. For Compressed Video ST 2110-22, consider the following
    * External ST 2110 compliant output device transmits compressed video.
    * Rx App 1, 2, 3, 4, 5, 6 receive uncompressed video frames.
