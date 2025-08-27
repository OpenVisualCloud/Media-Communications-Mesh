# Scenario Set 3 – SMPTE ST 2110 Outbound Transmission

## Scenario 3.3 – Single Node / Local Receiver

### Configuration

```mermaid
flowchart LR
    subgraph Single Node
        tx((Tx App))
        rx((Rx App))
        proxy(Media Proxy)
    end
    sw(["Network
        Switch"])
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy
    proxy --> rx
    proxy -- ST 2110 ----> sw
    sw -- ST 2110 --> dev
```

### Payload Options

* Video – SMPTE ST 2110-20 Uncompressed Video
* Video – SMPTE ST 2110-22 Compressed Video (JPEG XS)
* Audio – SMPTE ST 2110-30 Uncompressed Audio (PCM)

### Notes

1. For SMPTE ST 2110-22 Compressed Video, consider the following
    * Tx App transmits uncompressed video frames.
    * Rx App receives uncompressed video frames.
    * External SMPTE ST 2110 compliant input device receives compressed video.
