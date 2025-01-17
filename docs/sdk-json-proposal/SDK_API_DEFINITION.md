# Mesh Data Plane SDK Definition

## mesh_create_client()
```c
int mesh_create_client(MeshClient **client,
                       const char *config_json)
```
Creates a new mesh client with the provided JSON configuration.

#### Parameters
* `[OUT]` `client` – Address of a pointer to a mesh client structure.
* `[IN]` `config_json` – Pointer to a mesh client configuration string in JSON format.

#### Client Configuration Example
```json
{
  "apiVersion": "v1",
  "apiConnectionString": "Server=192.168.95.1; Port=8001",
  "apiDefaultTimeoutMicroseconds": 100000,
  "maxMediaConnections": 32,
}
```

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_delete_client()
```c
int mesh_delete_client(MeshClient **client)
```
Deletes the mesh client and its resources.

#### Parameters
* `[IN/OUT]` `client` – Address of a pointer to a mesh client structure.

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_create_tx_connection()
```c
int mesh_create_tx_connection(MeshClient *client,
                              MeshConnection **conn,
                              const char *config_json)
```
Creates a transmitter connection with the provided JSON configuration.

#### Parameters
* `[IN]` `client` – Pointer to a parent mesh client.
* `[OUT]` `conn` – Address of a pointer to the connection structure.
* `[IN]` `config_json` – Pointer to a connection configuration structure.

#### Transmitter/Receiver Connection Configuration Example
```json
{
  "bufferQueueCapacity": 16,
  "maxPayloadSize": 2097152,
  "maxMetadataSize": 8192,
  "connection": {
    "multipointGroup": {
      "urn": "ipv4:224.0.0.1:9003",
    },
    "st2110": {
      "transport": "st2110-20" | "st2110-22" | "st2110-30" | "...",
      "remoteIpAddr": "192.168.95.2",
      "remotePort": 9002,
      "pacing": "narrow",
      "payloadType": 112,
      "transportPixelFormat": "yuv422p10rfc4175"
    },
    "rdma": {
      "connectionMode": "RC" | "UC" | "UD" | "RD",
      "maxLatencyNanoseconds": 10000
    }
  },
  "payload": {
    "video": {
      "width": 1920,
      "height": 1080,
      "fps": 60.0,
      "pixelFormat": "yuv422p10rfc4175" | "yuv422p10le" | "v210"
    },
    "audio": {
      "channels": 2,
      "sampleRate": 48000 | 96000 | 44100,
      "format": "pcm_s24be" | "...",
      "packetTime": "1ms" | "..."
    },
    "ancillary": {
    },
    "blob": {
    }
  }
}
```
The `transportPixelFormat` is required only for st2110-20 transport type, otherwise shall not be present.

#### Returns
0 if successful. Otherwise, returns an error.

Allowed use cases of "transportPixelFormat" and "pixelFormat" for st2110-20

| pixelFormat       | transportPixelFormat | internal conversion | ffmpeg support | ffmpeg enum and string             |
|-------------------|----------------------|---------------------|----------------|------------------------------------|
| yuv422p10rfc4175  | yuv422p10rfc4175     | No                  | No             | N/A                                |
| yuv422p10le       | yuv422p10rfc4175     | Yes                 | Yes            | PIX_FMT_YUV422P10LE, "yuv422p10le" |
| v210              | yuv422p10rfc4175     | Yes                 | No             | N/A                                |

Allowed use cases of "pixelFormat" for st2110-22

| pixelFormat  | ffmpeg support | ffmpeg enum and string             |
|--------------|----------------|------------------------------------|
| yuv422p10le  | Yes            | PIX_FMT_YUV422P10LE, "yuv422p10le" |

Allowed use cases of "pixelFormat" for rdma

| pixelFormat  | ffmpeg support | ffmpeg enum and string             |
|--------------|----------------|------------------------------------|
| yuv422p10le  | Yes            | PIX_FMT_YUV422P10LE, "yuv422p10le" |
| v210         | No             | N/A                                |


## mesh_create_rx_connection()
```c
int mesh_create_rx_connection(MeshClient *client,
                              MeshConnection **conn,
                              const char *config_json)
```
Creates a receiver connection with the provided JSON configuration.

#### Parameters
* `[IN]` `client` – Pointer to a parent mesh client.
* `[OUT]` `conn` – Address of a pointer to the connection structure.
* `[IN]` `config_json` – Pointer to a connection configuration structure.

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_delete_connection()
```c
int mesh_delete_connection(MeshConnection **conn)
```
Deletes the connection and its resources.

#### Parameters
* `[IN/OUT]` `conn` – Address of a pointer to the connection structure.

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_get_buffer()
```c
int mesh_get_buffer(MeshConnection *conn,
                    MeshBuffer **buf)
```
Gets a buffer from the media connection.

#### Parameters
* `[IN]` `conn` – Pointer to a connection structure.
* `[OUT]` `buf` – Address of a pointer to a mesh buffer structure.

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_get_buffer_timeout()
```c
int mesh_get_buffer_timeout(MeshConnection *conn,
                            MeshBuffer **buf,
                            int timeout_us)
```
Gets a buffer from the media connection with a timeout.

#### Parameters
* `[IN]` `conn` – Pointer to a connection structure.
* `[OUT]` `buf` – Address of a pointer to a mesh buffer structure.
* `[IN]` `timeout_us` – Timeout interval in microseconds.

#### Returns
0 if successful. Otherwise, returns an error.


## MeshBuffer

```c
typedef struct{
    MeshConnection * const conn;
    void * const payload_ptr;
    const size_t payload_len;
    void * const metadata_ptr;
    const size_t metadata_len;
} MeshBuffer;
```
#### Fields
* `conn` – Pointer to a parent connection structure.
* `payload_ptr` – Pointer to shared memory area storing payload data.
* `payload_len` – Actual length of payload data in the buffer.
* `metadata_ptr` – Pointer to shared memory area storing metadata.
* `metadata_len` – Actual length of metadata in the buffer.



## mesh_put_buffer()
```c
int mesh_put_buffer(MeshBuffer **buf)
```
Puts the buffer to the media connection.

#### Parameters
* `[IN/OUT]` `buf` – Address of a pointer to a mesh buffer structure.

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_put_buffer_timeout()
```c
int mesh_put_buffer_timeout(MeshBuffer **buf,
                            int timeout_us)
```
Puts the buffer to the media connection with a timeout.

#### Parameters
* `[IN/OUT]` `buf` – Address of a pointer to a mesh buffer structure.
* `[IN]` `timeout_us` – Timeout interval in microseconds.

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_buffer_set_payload_len()
```c
int mesh_buffer_set_payload_len(MeshBuffer **buf,
                                size_t len)
```
Sets the payload length in the buffer provided by the media connection.

#### Parameters
* `[IN]` `buf` – Address of a pointer to a mesh buffer structure.
* `[IN]` `len` – Payload length.

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_buffer_set_metadata_len()
```c
int mesh_buffer_set_metadata_len(MeshBuffer **buf,
                                 size_t len)
```
Sets the metadata length in the buffer provided by the media connection.

#### Parameters
* `[IN]` `buf` – Address of a pointer to a mesh buffer structure.
* `[IN]` `len` – Metadata length.

#### Returns
0 if successful. Otherwise, returns an error.


## mesh_err2str()
```c
const char *mesh_err2str(int err)
```
Gets a text description of the error code.

#### Parameters
* `[IN]` `err` – Error code returned from any Mesh DP API call.

#### Returns
NULL-terminated string describing the error code.
