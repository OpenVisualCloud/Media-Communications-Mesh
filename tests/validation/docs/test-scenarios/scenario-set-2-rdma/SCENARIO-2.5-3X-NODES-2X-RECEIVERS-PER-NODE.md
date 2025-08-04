# Scenario Set 2 – RDMA Transmission

## Scenario 2.5 – 3x Nodes / 2x Receivers per Node

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
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- RDMA ----> sw1
    proxy1 -- RDMA ----> sw2
    sw1 -- RDMA --> proxy2
    proxy2 --> rxB1
    proxy2 --> rxB2
    sw2 -- RDMA --> proxy3
    proxy3 --> rxC1
    proxy3 --> rxC2
```

### Payload Options

* Blob
* Video – Uncompressed
* Audio

### Test Cases

For detailed test cases, refer to the centralized [Test Cases documentation](../SCENARIO.md#test-cases).
