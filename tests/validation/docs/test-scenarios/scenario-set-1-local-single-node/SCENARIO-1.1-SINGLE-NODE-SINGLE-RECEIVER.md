# Scenario Set 1 – Local Single Node Transmission (memif)

## Scenario 1.1 – Single Node / Single Receiver

### Configuration

```mermaid
flowchart
    subgraph Single Node
        tx((Tx App))
        rx((Rx App))
        proxy(Media Proxy)
    end
    tx --> proxy
    proxy --> rx
```

### Payload Options

* Blob
* Video – Uncompressed
* Audio

### Test Cases

For detailed test cases, refer to the centralized [Test Cases documentation](../SCENARIO.md#test-cases).
