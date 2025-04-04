# Scenario Set 4 – SMPTE ST 2110 Inbound Transmission

## Scenario 4.4 – 2x Nodes / 1x Receiver per Node

### Configuration

```mermaid
flowchart LR
    subgraph Node A
        rxA1((Rx App 1))
        proxy1(Media Proxy A)
    end
    subgraph Node B
        rxB1((Rx App 2))
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
    proxy1 -- RDMA ----> proxy2
    proxy2 --> rxB1
```

### Payload Options

* Video – Uncompressed ST 2110-20
* Video – Compressed ST 2110-22
* Audio – Uncompressed ST 2110-30

### Notes

1. For Compressed Video ST 2110-22, consider the following
    * External ST 2110 compliant output device transmits compressed video.
    * Rx App 1, 2 receive uncompressed video frames.
