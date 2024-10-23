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
0 on success; an error code otherwise.


## mesh_delete_client()
```c
int mesh_delete_client(MeshClient **client)
```
Deletes the mesh client and its resources.

#### Parameters
* `[IN/OUT]` `client` – Address of a pointer to a mesh client structure.

#### Returns
0 on success; an error code otherwise.


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
  "connType": "mediabuf" | "st2110" | "rdma",
  "connection": {
    "mediabuf": {
      "memifSocketPath": "mcm_socket_0",
      "memifInterfaceId": 0
    },
    "st2110": {
      "transport": "st2110-20" | "st2110-22" | "st2110-30" | "...",
      "remoteIpAddr": "192.168.95.2",
      "remotePort": "9002",
      "localIpAddr": "192.168.95.1",
      "localPort": "9001"
    },
    "rdma": {
      "remoteIpAddr": "192.168.95.2",
      "remotePort": "9002",
      "localIpAddr": "192.168.95.1",
      "localPort": "9001"
    }
  },
  "payloadType": "blob" | "video" | "audio" | "ancillary" | "...",
  "payload": {
    "video": {
      "width": 1920,
      "height": 1080,
      "fps": 60.0,
      "pixelFormat": "yuv422p10le" | "..."
    },
    "audio": {
      "channels": 2,
      "sampleRate": 48000 | 96000 | 44100,
      "format": "pcm_s24be" | "...",
      "packetTime": "1ms" | "..."
    }
  }
}
```

#### Returns
0 on success; an error code otherwise.


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
0 on success; an error code otherwise.


## mesh_establish_connection()
```c
int mesh_establish_connection(MeshConnection *conn,
                              int kind)
```
Applies the configuration to setup the connection payload for Audio packets.

#### Parameters
* `[IN]` `conn` – Pointer to a connection structure.
* `[IN]` `kind` – Connection kind: Sender or Receiver.

#### Returns
0 on success; an error code otherwise.


## mesh_shutdown_connection()
```c
int mesh_shutdown_connection(MeshConnection *conn)
```
Closes the active mesh connection.

#### Parameters
* `[IN]` `conn` – Pointer to a connection structure.

#### Returns
0 on success; an error code otherwise.


## mesh_delete_connection()
```c
int mesh_delete_connection(MeshConnection **conn)
```
Deletes the connection and its resources.

#### Parameters
* `[IN/OUT]` `conn` – Address of a pointer to the connection structure.

#### Returns
0 on success; an error code otherwise.


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
0 on success; an error code otherwise.


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
0 on success; an error code otherwise.


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
0 on success; an error code otherwise.


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
0 on success; an error code otherwise.


## mesh_err2str()
```c
const char *mesh_err2str(int err)
```
Gets a text description of the error code.

#### Parameters
* `[IN]` `err` – Error code returned from any Mesh Data Plane API call.

#### Returns
NULL-terminated string describing the error code.
