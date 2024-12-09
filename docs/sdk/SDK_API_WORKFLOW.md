# Mesh Data Plane SDK – API Workflow

## General workflow

1. Create a Mesh client:
   - `mesh_create_client()`
2. Create a Mesh connection:
   - `mesh_create_connection()`
3. Apply the connection configuration:
   - `struct MeshConfig_Memif {...}` -> `mesh_apply_config_memif()`
   - `struct MeshConfig_ST2110 {...}` -> `mesh_apply_config_st2110()`
   - `struct MeshConfig_RDMA {...}` -> `mesh_apply_config_rdma()`
4. Apply the payload configuration:
   - `struct MeshConfig_Video` -> `mesh_apply_config_video()`
   - `struct MeshConfig_Audio` -> `mesh_apply_config_audio()`
5. Establish the Mesh connection:
   - `mesh_establish_connection()`
6. Get a buffer from the Mesh connection:
   - `mesh_get_buffer()`
   - `mesh_get_buffer_timeout()`
7. Put the buffer to the Mesh connection:
   - `mesh_put_buffer()`
   - `mesh_put_buffer_timeout()`
8. Shutdown the Mesh connection:
   - `mesh_shutdown_connection()`
9. Delete the Mesh connection:
   - `mesh_delete_connection()`
10. Delete the Mesh client:
   - `mesh_delete_client()`

## Configure connection – Memif

1. Define a memif configuration:
   - `struct MeshConfig_Memif`
     - `.socket_path`
     - `.interface_id`
2. Apply the configuration:
   - `mesh_apply_config_memif()`

## Configure connection – SMPTE ST2110-XX

1. Define an SMPTE ST2110-XX configuration:
   - `struct MeshConfig_ST2110`
     - `.remote_ip_addr`
     - `.remote_port`
     - `.local_ip_addr`
     - `.local_port`
     - `.transport`
       - `MESH_CONN_TRANSPORT_ST2110_20` – SMPTE ST2110-20 Uncompressed Video
       - `MESH_CONN_TRANSPORT_ST2110_22` – SMPTE ST2110-22 Constant Bit-Rate Compressed Video
       - `MESH_CONN_TRANSPORT_ST2110_30` – SMPTE ST2110-30 Audio
2. Apply the configuration:
   - `mesh_apply_config_st2110()`

## Configure connection – RDMA

1. Define an RDMA configuration:
   - `struct MeshConfig_RDMA`
     - `.remote_ip_addr`
     - `.remote_port`
     - `.local_ip_addr`
     - `.local_port`
2. Apply the configuration:
   - `mesh_apply_config_rdma()`

## Configure payload – Video

1. Define a video configuration:
   - `struct MeshConfig_Video`
     - `.width`
     - `.height`
     - `.fps`
     - `.pixel_format` – `MESH_VIDEO_PIXEL_FORMAT_*`
2. Apply the configuration:
   - `mesh_apply_config_video()`

## Configure payload – Audio

1. Define an audio configuration:
   - `struct MeshConfig_Audio`
     - `.channels`
     - `.sample_rate` – `MESH_AUDIO_SAMPLE_RATE_*`
     - `.format` – `MESH_AUDIO_FORMAT_*`
     - `.packet_time` – `MESH_AUDIO_PACKET_TIME_*`
2. Apply the configuration:
   - `mesh_apply_config_audio()`

## Mesh DP SDK API internals – Connection configuration flowchart

```mermaid
flowchart LR
   kind(.kind) --> sender[_Sender_]
   kind --> receiver[_Receiver_]

   conn_type(.conn_type) --> memif_t[_memif_]
   conn_type --> st_t[_SMPTE ST2110-XX_]
   conn_type --> rdma_t[_RDMA_]

   conn(.conn) --> memif(.memif)
      memif --> socket_path(".socket_path
                             .interface_id")

   conn --> st(.st2110) --> st_param(".remote_ip_addr
                                      .remote_port
                                      .local_ip_addr
                                      .local_port")
            st ---> transport(.transport)
               transport --> st20["_SMPTE ST2110-20_
                  Uncompressed Video"]
               transport --> st22["_SMPTE ST2110-22_
                  Constant Bit-Rate Compressed Video"]
               transport --> st30["_SMPTE ST2110-30_
                  Audio"]

   conn --> rdma(.rdma) --> rdma_param(".remote_ip_addr
                                        .remote_port
                                        .local_ip_addr
                                        .local_port")

   payload_type(.payload_type) --> video_t(_Video_)
   payload_type --> audio_t(_Audio_)

   payload(.payload) --> video(.video) --> videoparam(".width
                                                       .height
                                                       .fps
                                                       .pixel_format")
   payload --> audio(.audio) --> audioparam(".channels
                                             .sample_rate
                                             .format
                                             .packet_time")
```

## Mesh DP SDK API internals – Connection configuration mindmap

```mermaid
mindmap
)__MeshConnection__ configuration(
   **.kind**
      (_Sender_)
      (_Receiver_)
   **.conn_type**
      (memif)
      (SMPTE ST2110-XX)
      (RDMA)
   **.conn**
      ((**.memif**))
         (.socket_path)
         (.interface_id)
      ((**.st2110**))
         (.remote_ip_addr)
         (.remote_port)
         (.local_ip_addr)
         (.local_port)
         (.transport)
            [_ST2110-20_]
            [_ST2110-22_]
            [_ST2110-30_]
      ((**.rdma**))
         (.remote_ip_addr)
         (.remote_port)
         (.local_ip_addr)
         (.local_port)
   **.payload_type**
      (_Video_)
      (_Audio_)
   **.payload**
      ((**.video**))
         (.width)
         (.height)
         (.fps)
         (.pixel_format)
      ((**.audio**))
         (.channels)
         (.sample_rate)
         (.format)
         (.packet_time)
```
