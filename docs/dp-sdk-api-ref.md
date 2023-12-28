# API User Guide for MCM Data Plane SDK

## mcm_create_connection
### Description
Creates a session for MCM data plane connection.

### Parameters
- `param`: Parameters for the connect session.

### Return Value
The context handler of the created connect session (`mcm_conn_context*`).

## mcm_destroy_connection
### Description
Destroys an MCM DP connection.

### Parameters
- `pctx`: The context handler of the connection to be destroyed.

### Return Value
None

## mcm_dequeue_buffer
### Description
Gets a buffer from the buffer queue.

- For the TX side, this function is used to allocate a buffer from the buffer queue.
- For the RX side, it is used to read a buffer from the TX side.

### Parameters
- `pctx`: The context handler of the created connect session.
- `timeout`: Timeout in milliseconds.
  - Passive event polling:
    - `timeout = 0`: Don't wait for an event, check the event queue if there is an event and return.
    - `timeout = -1`: Wait until an event occurs.
- `error_code`: Error code if failed, can be set to NULL if not required.

### Return Value
Pointer to the `mcm_buffer`. Returns `NULL` if failed.

## mcm_enqueue_buffer
### Description
Puts a buffer to the buffer queue.

- For the RX side, this function is used to return a buffer back to the queue.
- For the TX side, it is used to send a buffer to the RX side.

### Parameters
- `pctx`: The context handler of the created connect session.
- `buf`: Pointer to the `mcm_buffer`.

### Return Value
Error code if failed. Returns `0` if successful.

This user guide provides a brief overview of the functions and their usage. You can expand this document by adding more detailed information such as function usage examples, expected behavior, and any specific error handling details.
