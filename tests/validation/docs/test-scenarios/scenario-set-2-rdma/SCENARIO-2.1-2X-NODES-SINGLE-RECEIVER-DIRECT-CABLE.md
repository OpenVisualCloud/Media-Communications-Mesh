# Scenario Set 2 – RDMA Transmission

## Scenario 2.1 – 2x Nodes / Single Receiver – Direct Network Cable Connection

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
    tx --> proxy1
    proxy1 -- RDMA --> proxy2
    proxy2 --> rxB1
```

### Payload Options

* Blob
* Video – Uncompressed
* Audio
