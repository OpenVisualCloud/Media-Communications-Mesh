# SDK API Definition — Media Communications Mesh

Header file to include: [`mesh_dp.h`](../sdk/include/mesh_dp.h)

## mesh_create_client()
```c
int mesh_create_client(MeshClient **client,
                       const char *config_json)
```
Creates a new mesh client with the provided JSON configuration.

### Parameters
* `[OUT]` `client` – Address of a pointer to a mesh client structure.
* `[IN]` `config_json` – Pointer to a mesh client configuration string in JSON format.

### Client Configuration Example
```json
{
  "apiVersion": "v1",
  "apiConnectionString": "Server=127.0.0.1; Port=8002",
}
```

### JSON structure fields
* `"apiVersion"` – Default "v1".
* `"apiConnectionString"` – IP address and the port number of Media Proxy SDK API in the form of a connection string as in the example: "Server=127.0.0.1; Port=8002".
* `"apiDefaultTimeoutMicroseconds"` – Default timeout interval for SDK API calls, default 1000000.
* `"maxMediaConnections"` – Maximum number of media connections, default 32.

### Environment variables

* `MCM_MEDIA_PROXY_IP` – IP address of Media Proxy SDK API, default "127.0.0.1".
* `MCM_MEDIA_PROXY_PORT` – Port number of Media Proxy SDK API, default 8002.

### Note on configuring the Media Proxy address

There are two options to configure the IP address and the port number of Media Proxy SDK API, ordered by priority:
* Specify `"apiConnectionString"` in the JSON configuration string.
* Set `MCM_MEDIA_PROXY_IP` and `MCM_MEDIA_PROXY_PORT`.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_delete_client()
```c
int mesh_delete_client(MeshClient **client)
```
Deletes the mesh client and its resources.

### Parameters
* `[IN/OUT]` `client` – Address of a pointer to a mesh client structure.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_create_tx_connection()
```c
int mesh_create_tx_connection(MeshClient *client,
                              MeshConnection **conn,
                              const char *config_json)
```
Creates a transmitter connection with the provided JSON configuration.

### Parameters
* `[IN]` `client` – Pointer to a parent mesh client.
* `[OUT]` `conn` – Address of a pointer to the connection structure.
* `[IN]` `config_json` – Pointer to a connection configuration structure.

### Transmitter/Receiver Connection Configuration Example
```json
{
  "connection": {
    "st2110": {
      "transport": "st2110-20",
      "ipAddr": "224.0.0.1",
      "port": 9002,
      "multicastSourceIpAddr": "192.168.95.10",
      "pacing": "narrow",
      "payloadType": 112,
      "transportPixelFormat": "yuv422p10rfc4175"
    },
  },
  "options": {
    "rdma": {
      "provider": "tcp",
      "numEndpoints": 2,
    }
  },
  "payload": {
    "video": {
      "width": 1920,
      "height": 1080,
      "fps": 60.0,
      "pixelFormat": "yuv422p10rfc4175"
    },
  }
}
```

The following examples show optional parts of the JSON configuration that can be used
to setup various connection and payload options. The examples correspond to the main
(`.`) level of the JSON document.

### Configure connection – Multipoint Group
```json
"connection": {
  "multipointGroup": {
    "urn": "ipv4:224.0.0.1"
  }
}
```

### Configure connection – SMPTE ST 2110
```json
"connection": {
  "st2110": {
    "transport": "st2110-22",
    "ipAddr": "224.0.0.1",
    "multicastSourceIpAddr": "192.168.95.10",
    "port": 9002,
    "pacing": "narrow",
    "payloadType": 112
  }
}
```

### Configure connection options
```json
"options": {
  "rdma": {
    "provider": "tcp",
    "numEndpoints": 2,
  }
},
```

### Configure payload – Video
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

### Configure payload – Audio
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

### Configure payload – Blob Data
```json
"payload": {
  "blob": {}   
},
"maxPayloadSize": 2097152
```
The above `maxPayloadSize` is set explicitly to define the buffer size for the Blob payload.

### Configure optional parameters
```json
"bufferQueueCapacity": 16,
"maxMetadataSize": 8192,
"connCreationDelayMilliseconds": 100
```


### JSON structure fields
* `"bufferQueueCapacity"` – Dataplane buffer queue capacity between 2-128, default 16.
* `"maxPayloadSize"` – Payload maximum size. If set to 0, calculated automatically from the payload parameters at connection creation. Default 0.
* `"maxMetadataSize"` – User metadata maximum size, default 0.
* `"connCreationDelayMilliseconds"` – Delay between the connection creation and sending of the very first buffer, default 0.
* `"connection"` – Connection type, options 1-2 are the following:
   1. `"st2110"` – SMPTE ST 2110 connection.
      * `"transport"` – SMPTE ST 2110 connection type.
         * `"st2110-20"` – Uncompressed video.
         * `"st2110-22"` – Compressed video (JPEG XS).
         * `"st2110-30"` – Uncompressed audio (PCM).
      * `"ipAddr"` – **Tx connection**: destination IP address. **Rx connection**: multicast IP address or unicast source IP address.
      * `"port"` – **Tx connection**: destination port number. **Rx connection**: local port number.
      * `"multicastSourceIpAddr"` – **Rx connection only**: optional multicast source filter IP address.
      * `"pacing"`
         * `"narrow"`
      * `"payloadType"` – Default 112.
      * `"transportPixelFormat"` – Required only for the `"st2110-20"` transport type, default "yuv422p10rfc4175".
   1. `"multipointGroup"` – Multipoint Group connection.
      * `"urn"` – Uniform Resource Name (URN) of the multipoint group, e.g. "ipv4:224.0.0.1:9003".
* `"options"` – Connection options
   * `"rdma"` – RDMA bridge related parameters
      * `"provider"` – Default "tcp".
         * `"tcp"`
         * `"verbs"`
      * `"numEndpoints"` – Integer number of RDMA endpoints between 1-8, default 1.
* `"payload"` – Payload type, options 1-3 are the following:
   1. `"video"` – Video payload.
      * `"width"` – Integer frame width, e.g. 1920.
      * `"height"` – Integer frame height, e.g. 1080.
      * `"fps"` – Float frame rate, e.g. 60.0.
      * `"pixelFormat"` – Default "yuv422p10le".
         * `"yuv422p10le"`
         * `"v210"`
         * `"yuv422p10rfc4175"`
   1. `"audio"` – Audio payload.
      * `"channels"` – Default 2.
      * `"sampleRate"`
         * `48000`
         * `96000`
         * `44100`
      * `"format"` – Default "pcm_s24be".
         * `"pcm_s24be"`
         * `"pcm_s16be"`
         * `"pcm_s8"`
      * `"packetTime"` – Default "1ms".
         * `"1ms"` – Sample rate 48000 or 96000.
         * `"125us"` – Sample rate 48000 or 96000.
         * `"250us"` – Sample rate 48000 or 96000.
         * `"333us"` – Sample rate 48000 or 96000.
         * `"4ms"` – Sample rate 48000 or 96000.
         * `"80us"` – Sample rate 48000 or 96000.
         * `"1.09ms"` – Sample rate 44100.
         * `"0.14ms"` – Sample rate 44100.
         * `"0.09ms"` – Sample rate 44100.
    1. `"blob"` – Blob payload.

### Compatibility of connection types, transport types, and pixel formats
Allowed use cases of `"transportPixelFormat"` and `"pixelFormat"` for `"st2110-20"`

| "pixelFormat"      | "transportPixelFormat" | Internal conversion | FFmpeg support | FFmpeg enum and string             |
|--------------------|------------------------|---------------------|----------------|------------------------------------|
| "yuv422p10rfc4175" | "yuv422p10rfc4175"     | No                  | No             | N/A                                |
| "yuv422p10le"      | "yuv422p10rfc4175"     | Yes                 | Yes            | PIX_FMT_YUV422P10LE, "yuv422p10le" |
| "v210"             | "yuv422p10rfc4175"     | Yes                 | No             | N/A                                |

Allowed use cases of `"pixelFormat"` for `"st2110-22"`

| "pixelFormat"  | FFmpeg support | FFmpeg enum and string             |
|----------------|----------------|------------------------------------|
| "yuv422p10le"  | Yes            | PIX_FMT_YUV422P10LE, "yuv422p10le" |

Allowed use cases of `"pixelFormat"` for `"multipointGroup"`

| "pixelFormat" | FFmpeg support | FFmpeg enum and string             |
|---------------|----------------|------------------------------------|
| "yuv422p10le" | Yes            | PIX_FMT_YUV422P10LE, "yuv422p10le" |
| "v210"        | No             | N/A                                |

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## MeshConnection

```c
typedef struct MeshConnection {
    MeshClient * const client;
    const size_t payload_size;
    const size_t metadata_size;
} MeshConnection;
```
### Fields
* `client` – Pointer to a parent mesh client.
* `payload_size` – Maximum payload size, or frame size, configured for the connection.
* `metadata_size` – Maximum metadata size, configured for the connection.


## mesh_create_rx_connection()
```c
int mesh_create_rx_connection(MeshClient *client,
                              MeshConnection **conn,
                              const char *config_json)
```
Creates a receiver connection with the provided JSON configuration.

### Parameters
* `[IN]` `client` – Pointer to a parent mesh client.
* `[OUT]` `conn` – Address of a pointer to the connection structure.
* `[IN]` `config_json` – Pointer to a connection configuration structure.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_delete_connection()
```c
int mesh_delete_connection(MeshConnection **conn)
```
Deletes the connection and its resources.

### Parameters
* `[IN/OUT]` `conn` – Address of a pointer to the connection structure.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_get_buffer()
```c
int mesh_get_buffer(MeshConnection *conn,
                    MeshBuffer **buf)
```
Gets a buffer from the media connection.

The buffer must be returned back to the Mesh connection by calling `mesh_put_buffer()`
or `mesh_put_buffer_timeout()`.

### Parameters
* `[IN]` `conn` – Pointer to a connection structure.
* `[OUT]` `buf` – Address of a pointer to a mesh buffer structure.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_get_buffer_timeout()
```c
int mesh_get_buffer_timeout(MeshConnection *conn,
                            MeshBuffer **buf,
                            int timeout_us)
```
Gets a buffer from the media connection with a timeout.

The buffer must be returned back to the Mesh connection by calling `mesh_put_buffer()`
or `mesh_put_buffer_timeout()`.

### Parameters
* `[IN]` `conn` – Pointer to a connection structure.
* `[OUT]` `buf` – Address of a pointer to a mesh buffer structure.
* `[IN]` `timeout_us` – Timeout interval in microseconds.

### Timeout definition constants
* `MESH_TIMEOUT_DEFAULT` – Applies the default timeout interval defined for the Mesh client.
* `MESH_TIMEOUT_INFINITE` – No timeout, blocks until success or an error.
* `MESH_TIMEOUT_ZERO` – Polling mode, returns immediately.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


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
### Fields
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

The buffer must be previously obtained from to the Mesh connection by calling
`mesh_get_buffer()` or `mesh_get_buffer_timeout()`.

In case of Tx connection, a call to this API function triggers transmittion
of the buffer from SDK to Media Proxy and then to the receiver over the Mesh.

### Parameters
* `[IN/OUT]` `buf` – Address of a pointer to a mesh buffer structure.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_put_buffer_timeout()
```c
int mesh_put_buffer_timeout(MeshBuffer **buf,
                            int timeout_us)
```
Puts the buffer to the media connection with a timeout.

The buffer must be previously obtained from to the Mesh connection by calling
`mesh_get_buffer()` or `mesh_get_buffer_timeout()`.

In case of Tx connection, a call to this API function triggers transmittion
of the buffer from SDK to Media Proxy and then to the receiver over the Mesh.

### Parameters
* `[IN/OUT]` `buf` – Address of a pointer to a mesh buffer structure.
* `[IN]` `timeout_us` – Timeout interval in microseconds.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_buffer_set_payload_len()
```c
int mesh_buffer_set_payload_len(MeshBuffer *buf,
                                size_t len)
```
Sets the payload length in the buffer provided by the media connection.

### Parameters
* `[IN]` `buf` – Pointer to a mesh buffer structure.
* `[IN]` `len` – Payload length in bytes.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_buffer_set_metadata_len()
```c
int mesh_buffer_set_metadata_len(MeshBuffer *buf,
                                 size_t len)
```
Sets the metadata length in the buffer provided by the media connection.

### Parameters
* `[IN]` `buf` – Pointer to a mesh buffer structure.
* `[IN]` `len` – Metadata length in bytes.

### Returns
0 if successful. Otherwise, returns an [Error code](#return-error-codes).


## mesh_err2str()
```c
const char * mesh_err2str(int err)
```
Gets a text description of the error code.

### Parameters
* `[IN]` `err` – Error code returned from any Mesh DP API call.

### Returns
NULL-terminated string describing the error code.


## Return error codes

API SDK functions may return the following error codes.

NOTE: The codes are negative integer values.

| Error code macro name            | Description                    | Meaning                            |
|----------------------------------|--------------------------------|------------------------------------|
| `-MESH_ERR_BAD_CLIENT_PTR`       | Bad client pointer             | The client pointer is NULL.        |
| `-MESH_ERR_BAD_CONN_PTR`         | Bad connection pointer         | The connection pointer is NULL.    |
| `-MESH_ERR_BAD_CONFIG_PTR`       | Bad configuration pointer      | The configuration pointer is NULL. |
| `-MESH_ERR_BAD_BUF_PTR`          | Bad buffer pointer             | The buffer pointer is NULL.        |
| `-MESH_ERR_BAD_BUF_LEN`          | Bad buffer length              | **Rx connection**: The buffer length is corrupted.<br>**Tx connection**: The buffer length is bigger than maximum. |
| `-MESH_ERR_CLIENT_CONFIG_INVAL`  | Invalid client config          | JSON client configuration string is malformed. |
| `-MESH_ERR_MAX_CONN`             | Reached max connections number | An attempt to create a connection failed due to reaching the maximum number of connections defined in `"maxMediaConnections"`. |
| `-MESH_ERR_FOUND_ALLOCATED`      | Found allocated resources      | When deleting a client, some connections were found not closed. Delete all connections explicitly before deleting the client. |
| `-MESH_ERR_CONN_FAILED`          | Connection creation failed     | An error occurred while creating a connection. |
| `-MESH_ERR_CONN_CONFIG_INVAL`    | Invalid connection config      | JSON connection configuration string is malformed or one of parameters has an incorrect value. |
| `-MESH_ERR_CONN_CONFIG_INCOMPAT` | Incompatible connection config | Incompatible parameters found in the JSON connection configuration string. |
| `-MESH_ERR_CONN_CLOSED`          | Connection is closed           | When getting a buffer, the connection appeared to become closed. |
| `-MESH_ERR_TIMEOUT`              | Timeout occurred               | Timeout occurred when performing an operation. |


<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
