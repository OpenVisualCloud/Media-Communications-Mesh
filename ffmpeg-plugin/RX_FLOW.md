# Rx flow

Diagrams in this document show interaction between FFmpeg and the MCM plugin when FFmpeg is receiving a video stream over MCM.

### Simplified FFmpeg Rx flow
```mermaid
sequenceDiagram
   participant ffmpeg as FFmpeg
   participant mux as MCM Plugin

   ffmpeg ->> mux: Read header
   loop N video frames
   ffmpeg ->>+ mux: Read packet
   mux ->> ffmpeg: Packet received
   mux -->>- ffmpeg: EOF
   end
   ffmpeg ->> mux: Read close
```

### FFmpeg Read Header flow
```mermaid
sequenceDiagram
   participant ffmpeg as FFmpeg
   participant mux as MCM Plugin
   participant sdk as MCM SDK
   participant memif as Memif
   participant proxy as MCM Media Proxy
   participant mtl as MTL
   participant network as Network

   ffmpeg ->>+ mux: Read header
   mux ->>+ sdk: Create Rx connection
   sdk ->>+ proxy: Open TCP connection to Proxy
   proxy ->>+ mtl: Start Rx session
   mtl ->>+ network: Update ARP table
   mtl ->>- proxy: Session started
   proxy ->>+ memif: Create Memif Tx connection
   memif ->>- proxy: Connection created
   proxy ->>- sdk: Connection established

   sdk ->>+ proxy: Query Memif info
   proxy ->>- sdk: Memif info

   sdk ->>+ memif: Create Memif Rx connection
   memif ->>- sdk: Connection created
   sdk ->>- mux: Connection created
   mux ->>- ffmpeg: Success
```

### FFmpeg Read Packet flow
```mermaid
sequenceDiagram
   participant ffmpeg as FFmpeg
   participant mux as MCM Plugin
   participant sdk as MCM SDK
   participant memif as Memif
   participant proxy as MCM Media Proxy
   participant mtl as MTL
   participant network as Network

   network ->>+ mtl: Data received
   mtl ->> mtl: Decode ST2110-xx
   mtl ->>- proxy: Frame received
   proxy ->>+ memif: Dequeue buffer
   memif ->>- proxy: Buffer dequeued
   proxy ->> proxy: Write data to buffer
   proxy ->> memif: Enqueue buffer

   ffmpeg ->>+ mux: Read packet
   mux ->>+ sdk: Receive buffer
   sdk ->>+ memif: Dequeue buffer
   memif ->>- sdk: Buffer dequeued
   sdk ->>- mux: Buffer received
   mux ->> mux: Read frame from buffer
   mux ->>+ sdk: Release buffer
   sdk ->>+ memif: Enqueue buffer
   memif ->>- sdk: Buffer enqueued
   sdk ->>- mux: Buffer released
   mux ->>- ffmpeg: Success
```

### FFmpeg Read Close flow
```mermaid
sequenceDiagram
   participant ffmpeg as FFmpeg
   participant mux as MCM Plugin
   participant sdk as MCM SDK
   participant memif as Memif
   participant proxy as MCM Media Proxy
   participant mtl as MTL
   participant network as Network

   ffmpeg ->>+ mux: Read close
   mux ->>+ sdk: Close Rx connection
   sdk ->>+ proxy: Close TCP connection to Proxy
   proxy ->> mtl: Stop Rx session
   proxy ->>- memif: Close Memif Tx connection
   sdk ->> memif: Close Memif Rx connection
   sdk ->>- mux: Connection closed
   mux ->>- ffmpeg: Return
```
