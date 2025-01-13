# Mesh Data Plane SDK – Example / SMPTE ST2110-22 RX Flow

The diagram in this document shows interaction between a user app and Mesh Data Plane in the video receive mode over SMPTE ST2110-22.

## RX flow stages
1. Create client.
1. Create connection.
1. Configure connection.
1. Establish Rx connection.
1. Receive video frames.
1. Shutdown connection.
1. Delete connection.
1. Delete client.

```mermaid
sequenceDiagram
   participant app as User Rx App
   participant sdk as Mesh DP SDK
   participant memif as Memif
   participant proxy as Media Proxy
   participant mtl as MTL
   participant network as Network

   note over app, network: Create client
   app ->>+ sdk: Create client
   sdk ->>- app: Client created

   note over app, network: Create connection
   app ->>+ sdk: Create connection
   sdk ->> sdk: Register connection in the client
   sdk ->>- app: Connection created

   note over app, network: Configure connection
   app ->> sdk: Apply SMPTE ST2110-XX configuration
   app ->> sdk: Apply video configuration

   note over app, network: Establish Rx connection
   app ->>+ sdk: Establish Rx connection
   sdk ->>+ proxy: Open TCP connection to Proxy
   proxy ->>+ mtl: Start Rx session
   note right of mtl: SMPTE ST2110-22
   mtl ->>+ network: Update ARP table
   mtl ->>- proxy: Session started
   proxy ->>+ memif: Create Memif Tx connection
   memif ->>- proxy: Connection created
   proxy ->>- sdk: Connection established

   sdk ->>+ proxy: Query Memif info
   proxy ->>- sdk: Memif info

   sdk ->>+ memif: Create Memif Rx connection
   memif ->>- sdk: Connection created
   sdk ->>- app: Connection established

   note over app, network: Receive video frames
   loop

   note right of mtl: SMPTE ST2110-22
   network ->>+ mtl: Data received
   mtl ->> mtl: Decode ST2110-22
   mtl ->>- proxy: Frame received
   proxy ->>+ memif: Dequeue buffer
   memif ->>- proxy: Buffer dequeued
   proxy ->> proxy: Write data to buffer
   proxy ->> memif: Enqueue buffer

   app ->>+ sdk: Get buffer
   alt
      sdk ->>+ memif: Dequeue buffer
      memif ->>- sdk: Buffer dequeued
      sdk ->> app: Buffer received
   else
      sdk -->>- app: Connection closed
      note over app, sdk: No more frames, exit the loop
   end

   app ->> app: Read frame from buffer
   app ->>+ sdk: Put buffer
   sdk ->>+ memif: Enqueue buffer
   memif ->>- sdk: Buffer enqueued
   sdk ->>- app: Buffer released

   end

   note over app, network: Shutdown connection
   app ->>+ sdk: Shutdown connection
   sdk ->>+ proxy: Close TCP connection to Proxy
   proxy ->> mtl: Stop Rx session
   note right of mtl: SMPTE ST2110-22
   mtl ->> network: Terminate connection
   proxy ->>- memif: Close Memif Tx connection
   sdk ->> memif: Close Memif Rx connection
   sdk ->>- app: Connection closed

   note over app, network: Delete connection
   app ->>+ sdk: Delete connection
   sdk ->> sdk: Unregister connection in the client
   sdk ->>- app: Connection deleted

   note over app, network: Delete client
   app ->>+ sdk: Delete client
   sdk ->>- app: Client deleted

```
