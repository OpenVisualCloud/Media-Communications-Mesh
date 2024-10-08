# Tx flow

Diagrams in this document show interaction between FFmpeg and the MCM plugin when FFmpeg is streaming a video file over MCM.

## Simplified FFmpeg Tx flow

```mermaid
sequenceDiagram
   participant ffmpeg as FFmpeg
   participant mux as MCM Plugin

   ffmpeg ->> ffmpeg: Open video file
   ffmpeg ->> mux: Write header
   loop N video frames
   ffmpeg ->> mux: Write packet
   end
   ffmpeg ->> mux: Write trailer
   ffmpeg ->> ffmpeg: Close video file
```

## FFmpeg Write Header flow

```mermaid
sequenceDiagram
   participant ffmpeg as FFmpeg
   participant mux as MCM Plugin
   participant sdk as MCM SDK
   participant memif as Memif
   participant proxy as MCM Media Proxy
   participant mtl as MTL
   participant network as Network

   ffmpeg ->>+ mux: Write header
   mux ->>+ sdk: Create Tx connection
   sdk ->>+ proxy: Open TCP connection to Proxy
   proxy ->>+ mtl: Start Tx session
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
   sdk ->>- mux: Connection created
   mux ->>- ffmpeg: Success
```

## FFmpeg Write Packet flow

```mermaid
sequenceDiagram
   participant ffmpeg as FFmpeg
   participant mux as MCM Plugin
   participant sdk as MCM SDK
   participant memif as Memif
   participant proxy as MCM Media Proxy
   participant mtl as MTL
   participant network as Network

   ffmpeg ->>+ mux: Write packet
   mux ->>+ sdk: Allocate buffer
   sdk ->>+ memif: Dequeue buffer
   memif ->>- sdk: Buffer dequeued
   sdk ->>- mux: Buffer allocated
   mux ->> mux: Write frame into buffer
   mux ->>+ sdk: Send buffer
   sdk ->>+ memif: Enqueue buffer
   memif ->>- sdk: Buffer enqueued
   sdk ->>- mux: Buffer sent
   mux ->>- ffmpeg: Success

   proxy ->>+ memif: Dequeue buffer
   memif ->>- proxy: Buffer dequeued
   proxy ->>+ mtl: Send frame
   mtl ->> mtl: Encode ST2110-xx
   mtl ->>- network: Transmit data
```

## FFmpeg Write Trailer flow

```mermaid
sequenceDiagram
   participant ffmpeg as FFmpeg
   participant mux as MCM Plugin
   participant sdk as MCM SDK
   participant memif as Memif
   participant proxy as MCM Media Proxy
   participant mtl as MTL
   participant network as Network

   ffmpeg ->>+ mux: Write trailer
   mux ->>+ sdk: Close Tx connection
   sdk ->>+ proxy: Close TCP connection to Proxy
   proxy ->> mtl: Stop Tx session
   proxy ->>- memif: Close Memif Rx connection
   sdk ->> memif: Close Memif Tx connection
   sdk ->>- mux: Connection closed
   mux ->>- ffmpeg: Return
```
