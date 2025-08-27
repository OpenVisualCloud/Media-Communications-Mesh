# Scenario Set 1 – Local Single Node Transmission (memif)

## Scenario 1.2 – Single Node / 3x Receivers

### Configuration

```mermaid
flowchart
    subgraph Single Node
        tx((Tx App))
        rx1((Rx App 1))
        rx2((Rx App 2))
        rx3((Rx App 3))
        proxy(Media Proxy)
    end
    tx --> proxy
    proxy --> rx1
    proxy --> rx2
    proxy --> rx3
```

### Payload Options

* Blob
* Video – Uncompressed
* Audio

### Test Cases

For detailed test cases, refer to the centralized [Test Cases documentation](../SCENARIO.md#test-cases).
