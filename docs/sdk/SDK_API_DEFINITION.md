# Mesh Data Plane SDK Definition

## mesh_create_client()
```c
int mesh_create_client(MeshClient **mc,
                       MeshClientConfig *cfg)
```
Creates a new mesh client from the given configuration structure.

#### Parameters
* `[OUT]` `mc` – Address of a pointer to a mesh client structure.
* `[IN]` `cfg` – Pointer to a mesh client configuration structure.

#### Returns
0 on success; an error code otherwise.


## mesh_delete_client()
```c
int mesh_delete_client(MeshClient **mc)
```
Deletes the mesh client and its resources.

#### Parameters
* `[IN/OUT]` `mc` – Address of a pointer to a mesh client structure.

#### Returns
0 on success; an error code otherwise.


## mesh_create_connection()
```c
int mesh_create_connection(MeshClient *mc,
                           MeshConnection **conn)
```
Creates a new media connection for the given mesh client.

#### Parameters
* `[IN]` `mc` – Pointer to a parent mesh client.
* `[OUT]` `conn` – Address of a pointer to the connection structure.

#### Returns
0 on success; an error code otherwise.


## mesh_apply_connection_config_memif()
```c
int mesh_apply_connection_config_memif(MeshConnection *conn,
                                       MeshConfig_Memif *cfg)
```
Applies the configuration to setup a single node direct connection via memif.

#### Parameters
* `[IN]` `conn` – Pointer to a connection structure.
* `[IN]` `cfg` – Pointer to a configuration structure.

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
                            int timeout_ms)
```
Gets a buffer from the media connection with a timeout.

#### Parameters
* `[IN]` `conn` – Pointer to a connection structure.
* `[OUT]` `buf` – Address of a pointer to a mesh buffer structure.
* `[IN]` `timeout_ms` – Timeout interval in milliseconds.

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
                            int timeout_ms)
```
Puts the buffer to the media connection with a timeout.

#### Parameters
* `[IN/OUT]` `buf` – Address of a pointer to a mesh buffer structure.
* `[IN]` `timeout_ms` – Timeout interval in milliseconds.

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
