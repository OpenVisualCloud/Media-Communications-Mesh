# Mesh Data Plane SDK – Example / SMPTE ST2110-22 Tx Flow

The diagram in this document shows interaction between a user app and Mesh Data Plane in the video transmit mode over SMPTE ST2110-22.

### Tx flow stages
1. Create client.
1. Create connection.
1. Configure connection.
1. Establish Tx connection.
1. Send N video frames.
1. Shutdown connection.
1. Delete connection.
1. Delete client.

```mermaid
sequenceDiagram
   participant app as User Tx App
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

   note over app, network: Establish Tx connection
   app ->>+ sdk: Establish Tx connection
   sdk ->>+ proxy: Open TCP connection to Proxy
   proxy ->>+ mtl: Start Tx session
   note right of mtl: SMPTE ST2110-22
   mtl ->>+ network: Connect to remote host
   network ->>- mtl: Connection established
   mtl ->>- proxy: Session started
   proxy ->>+ memif: Create Memif Rx connection
   memif ->>- proxy: Connection created
   proxy ->>- sdk: Connection established

   sdk ->>+ proxy: Query Memif info
   proxy ->>- sdk: Memif info

   sdk ->>+ memif: Create Memif Tx connection
   memif ->>- sdk: Connection created
   sdk ->>- app: Connection established

   note over app, network: Send N video frames
   loop

   app ->>+ sdk: Get buffer
   sdk ->>+ memif: Dequeue buffer
   memif ->>- sdk: Buffer dequeued
   sdk ->>- app: Buffer allocated
   app ->> app: Write frame into buffer
   app ->>+ sdk: Put buffer
   sdk ->>+ memif: Enqueue buffer
   memif ->>- sdk: Buffer enqueued
   sdk ->>- app: Buffer sent

   proxy ->>+ memif: Dequeue buffer
   memif ->>- proxy: Buffer dequeued
   proxy ->>+ mtl: Send frame
   note right of mtl: SMPTE ST2110-22
   mtl ->> mtl: Encode ST2110-22
   mtl ->>- network: Transmit data

   end

   note over app, network: Shutdown connection
   app ->>+ sdk: Shutdown connection
   sdk ->>+ proxy: Close TCP connection to Proxy
   proxy ->> mtl: Stop Tx session
   note right of mtl: SMPTE ST2110-22
   mtl ->> network: Terminate connection
   proxy ->>- memif: Close Memif Rx connection
   sdk ->> memif: Close Memif Tx connection
   sdk ->>- app: Connection closed

   note over app, network: Delete connection
   app ->>+ sdk: Delete connection
   sdk ->> sdk: Unregister connection in the client
   sdk ->>- app: Connection deleted

   note over app, network: Delete client
   app ->>+ sdk: Delete client
   sdk ->>- app: Client deleted

```
