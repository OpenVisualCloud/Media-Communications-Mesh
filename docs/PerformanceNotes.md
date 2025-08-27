# Performance Notes — Media Communications Mesh

## Configuration options

SDK API provides several configuration options to tune the Mesh performance.
The options are fully supported by the FFmpeg plugin as well.
See the following table for details.

| Configuration option | SDK API parameter | FFmpeg plugin CLI argument |
|----------------------|-------------------|----------------------------|
| **Connection delay** – Delay between connection creation and transmitting the very first buffer. The integer value should be specified in milliseconds. A 100 ms delay should satisfy most of the cases. Default 0.| `"connCreationDelayMilliseconds"`<br>*Example:*<br>{&nbsp;"connCreationDelayMilliseconds":&nbsp;100&nbsp;} | `-conn_delay`<br>*Example:*<br>&#8209;conn_delay&nbsp;100 |
| **Queue capacity** – Buffer queue capacity of the memif connection between SDK and Media Proxy. Accepted values are 2, 4, 8, 16, 32, 64, or 128. Default 16. | `"bufferQueueCapacity"`<br>*Example:*<br>{&nbsp;"bufferQueueCapacity":&nbsp;64&nbsp;} | `-buf_queue_cap`<br>*Example:*<br>&#8209;buf_queue_cap&nbsp;64 |

## Known performance issues – How to resolve

### *4K 60fps Video* – Initial buffer alignment when connecting a receiver to an active stream

For a 4K 60fps video stream, which typically requires a bandwidth exceeding 20 Gbit/s,
the system is designed to handle 60 frames, or buffers, per second. The default buffer
queue capacity of 16 is suitable for most scenarios. However, when a receiver is connected,
the Mesh Agent requires a brief interval, typically 30-80 ms, to configure the network
topology for traffic delivery.

To ensure seamless startup without missing initial frames, consider configuring
the following parameters
* **Sender side** – Specify a **connection delay** of 100-300 ms.
* **Receiver side** – Increase the buffer **queue capacity** to 32 or 64.

### *4K 60fps Video* – Initial buffer alignment when a sender begins transmitting to a Multipoint Group

For streams being sent to a Multipoint Group, applying similar configuration settings
can help ensure a smooth start for all receivers
* **Sender side** – Specify a **connection delay** of 100-300 ms.
* **Receiver side** – Increase the buffer **queue capacity** to 32 or 64.

### *Audio* – Packets drop when receiving an active stream

Audio streams have smaller bandwidth than video but the packets come to receivers much
faster, at least 1000 times per second depending on the packet time configuration.

To resolve that, adjust the following configuration option
* **Receiver side** – Increase the buffer **queue capacity** to 32, 64, or 128.

### *Audio* – First packets drop when connecting a receiver to an active stream

Set the following configuration options
* **Sender side** – Specify a **connection delay** of 100-300 ms.
* **Receiver side** – Increase the buffer **queue capacity** to 32, 64, or 128.

### *Audio* – First packets drop when a sender begins transmitting to a Multipoint Group

Same recommendation here as above
* **Sender side** – Specify a **connection delay** of 100-300 ms.
* **Receiver side** – Increase the buffer **queue capacity** to 32, 64, or 128.

<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
