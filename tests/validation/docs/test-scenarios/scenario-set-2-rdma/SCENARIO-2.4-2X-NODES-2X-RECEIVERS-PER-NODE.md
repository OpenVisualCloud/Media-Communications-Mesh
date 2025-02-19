# Scenario Set 2 – RDMA Transmission

## Scenario 2.4 – 2x Nodes / 2x Receivers per Node

### Configuration

```mermaid
flowchart LR
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
    sw(["Network
        Switch"])
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- RDMA ----> sw
    sw -- RDMA --> proxy2
    proxy2 --> rxB1
    proxy2 --> rxB2
```

### Payload Options

* Blob
* Video – Uncompressed
* Audio
