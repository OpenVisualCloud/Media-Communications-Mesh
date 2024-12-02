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
1. Define a memif configuration in JSON format:
```json
"connType": "memif",
"connection": {
  "memif": {
    "socketPath": "mcm_socket_0",
    "interfaceId": 0
  }
}
```

## Configure Connection – Memif
1. Define a memif configuration in JSON format:
```json
"connType": "memif",
"connection": {
  "memif": {
    "socketPath": "mcm_socket_0",
    "interfaceId": 0
  }
}
```

## Configure Connection – SMPTE ST2110-XX
1. Define an SMPTE ST2110-XX configuration:
```json
"connType": "st2110",
"connection": {
  "st2110": {
    "transport": "st2110-22",
    "remoteIpAddr": "192.168.95.2",
    "remotePort": "9002",
    "localIpAddr": "192.168.95.1",
    "localPort": "9001"
  }
}
```

## Configure Connection – RDMA
1. Define an RDMA configuration:
```json
"connType": "rdma",
"connection": {
  "rdma": {
    "remoteIpAddr": "192.168.95.2",
    "remotePort": "9002",
    "localIpAddr": "192.168.95.1",
    "localPort": "9001"
  }
}
```

## Configure Payload – Video
1. Define a video configuration:
```json
"payloadType": "video",
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
"payloadType": "audio",
"payload": {
  "audio": {
    "channels": 2,
    "sampleRate": 48000,
    "format": "pcm_s24be",
    "packetTime": "1ms"
  }
}
```

## Configure payload – Raw data
```json
"payloadType": "raw",
"maxPayloadSize": 2097152
```

## Configure metadata
```json
"maxMetadataSize": 8192
```

## Mesh DP SDK API Internals – Connection Configuration flowchart

```mermaid
flowchart LR

   conn_type(connectionType) --> group_t[_Multipoint Group_]
   conn_type --> memif_t[_memif_]
   conn_type --> st_t[_SMPTE ST2110-XX_]
   conn_type --> rdma_t[_RDMA_]

   conn(connection) --> group(Multipoint Group)
      group --> group_addr("ipAddr
                            port
                            —**OR**—
                            urn")

   conn --> memif(memif)
      memif --> socket_path("socketPath
                             interfaceId")

   conn --> st(st2110) --> st_param("remoteIpAddr
                                     remotePort
                                     localIpAddr
                                     localPort")
            st ---> transport(transport)
               transport --> st20["_SMPTE ST2110-20_ 
                  Uncompressed Video"]
               transport --> st22["_SMPTE ST2110-22_
                  Constant Bit-Rate Compressed Video"]
               transport --> st30["_SMPTE ST2110-30_
                  Audio"]

   conn --> rdma(rdma) --> rdma_param("remoteIpAddr
                                       remotePort
                                       localIpAddr
                                       localPort")

   payload_type(payloadType) --> video_t(_Video_)
   payload_type --> audio_t(_Audio_)

   payload(payload) --> video(video) --> videoparam("width
                                                     height
                                                     fps
                                                     pixelFormat")
   payload --> audio(audio) --> audioparam("channels
                                            sampleRate
                                            format
                                            packetTime")
```
