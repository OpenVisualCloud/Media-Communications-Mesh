# Mesh Data Plane SDK – API Workflow
## General Workflow
1. Create a Mesh client:
   * `mesh_create_client()`
1. Create a Mesh Tx or Rx connection:
   * `mesh_create_tx_connection()`
   * `mesh_create_rx_connection()`
1. Get a buffer from the Mesh connection:
   * `mesh_get_buffer()`
   * `mesh_get_buffer_timeout()`
1. Put the buffer to the Mesh connection:
   * `mesh_put_buffer()`
   * `mesh_put_buffer_timeout()`
1. Shutdown the Mesh connection:
   * `mesh_shutdown_connection()`
1. Delete the Mesh connection:
   * `mesh_delete_connection()`
1. Delete the Mesh client:
   * `mesh_delete_client()`

## Configure Connection – Multipoint Group
```json
"connection": {
  "multipointGroup": {
    "ipAddr": "224.0.0.1",
    "port": "9003"
  }
}
```

## Configure Connection – SMPTE ST2110-XX
```json
"connection": {
  "st2110": {
    "transport": "st2110-22",
    "remoteIpAddr": "192.168.95.2",
    "remotePort": 9002,
    "pacing": "narrow",
    "payloadType": 112
  }
}
```

## Configure Connection – RDMA
```json
"connection": {
  "rdma": {
    "connectionMode": "RC",
    "maxLatencyNanoseconds": 10000
  }
}
```

## Configure Payload – Video
```json
"payload": {
  "video": {
    "width": 1920,
    "height": 1080,
    "fps": 60.0,
    "pixelFormat": "yuv422p10le"
  }
}
```

## Configure Payload – Audio
```json
"payload": {
  "audio": {
    "channels": 2,
    "sampleRate": 48000,
    "format": "pcm_s24be",
    "packetTime": "1ms"
  }
}
```

## Configure Payload – Blob Data
```json
"payload": {
  "blob": {
   ...
  }   
},
"maxPayloadSize": 2097152
```
The `maxPayloadSize` is set above explicitly to define the buffer size for Blob.

## Configure Optional Parameters
```json
"bufferQueueCapacity": 16,
"maxMetadataSize": 8192
```

## Mesh DP SDK API Internals – Connection Configuration Flowchart

```mermaid
flowchart LR

   conn_type(connectionType) --> group_t[_Multipoint Group_]
   conn_type --> st_t[_SMPTE ST2110-XX_]
   conn_type --> rdma_t[_RDMA_]

   conn(connection) --> group(Multipoint Group)
      group --> group_addr("urn")

   conn --> st(st2110) --> st_param("remoteIpAddr
                                     remotePort
                                     pacing
                                     payloadType")
            st ---> transport(transport)
               transport --> st20["_SMPTE ST2110-20_ 
                  Uncompressed Video"]
               transport --> st22["_SMPTE ST2110-22_
                  Constant Bit-Rate Compressed Video"]
               transport --> st30["_SMPTE ST2110-30_
                  Audio"]

   conn --> rdma(rdma) --> rdma_param("connectionMode
                                       maxLatencyNanoseconds")

   payload(payload) --> video(video) --> videoparam("width
                                                     height
                                                     fps
                                                     pixelFormat")
   payload --> audio(audio) --> audioparam("channels
                                            sampleRate
                                            format
                                            packetTime")

   payload --> aux(ancillary) --> auxparam("*TBD*")

   payload --> blob(blob) --> blobparam("*TBD*")
```
