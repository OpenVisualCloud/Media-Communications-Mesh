# Mesh Data Plane SDK – Examples

## Video Sender Example

```c
int main(void)
{
    /* Default client configuration */
    const char *client_config = R"(
      {
        "apiVersion": "v1",
        "apiConnectionString": "Server=192.168.96.1; Port=8001",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
      }
    )";

    // Transmitter connection configuration
    const char *conn_config = R"(
      {
        "connType": "multipoint-group",
        "connection": {
          "multipoint-group": {
            "ipAddr": "224.0.0.1",
            "port": "9003"
          }
        },
        "payloadType": "video",
        "payload": {
          "video": {
            "width": 1920,
            "height": 1080,
            "fps": 60,
            "pixelFormat": "yuv422p10le"
          }
        },
      }
    )";

    MeshConnection *conn;
    MeshClient *mc;
    int err;
    int i, n;

    /* Create a mesh client */
    err = mesh_create_client(&mc, client_config);
    if (err) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(1);
    }

    /* Create a mesh transmitter connection */
    err = mesh_create_tx_connection(mc, &conn, conn_config);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_client;
    }

    /* 10 video frames to be sent */
    n = 10;

    /* Send data loop */
    for (i = 0; i < n; i++) {
        MeshBuffer *buf;

        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(conn, &buf);
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        /* Fill the buffer with user data */
        put_user_video_frames(buf->payload_ptr, buf->payload_len);

        /* Send the buffer */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
    }

    /* Shutdown the connection */
    err = mesh_shutdown_connection(&conn);
    if (err)
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);

exit_delete_conn:
    /* Delete the media connection */
    mesh_delete_connection(&conn);

exit_delete_client:
    /* Delete the mesh client */
    mesh_delete_client(&mc);

    if (err)
        exit(1);
}
```

## Video Receiver Example
```c
int main(void)
{
    /* Default client configuration */
    const char *client_config = R"(
      {
        "apiVersion": "v1",
        "apiConnectionString": "Server=192.168.96.1; Port=8002",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
      }
    )";

    // Receiver connection configuration
    const char *conn_config = R"(
      {
        "connType": "multipoint-group",
        "connection": {
          "multipoint-group": {
            "ipAddr": "224.0.0.1",
            "port": "9003"
          }
        },
        "payloadType": "video",
        "payload": {
          "video": {
            "width": 1920,
            "height": 1080,
            "fps": 60,
            "pixelFormat": "yuv422p10le"
          }
        }
      }
    )";

    MeshConnection *conn;
    MeshClient *mc;
    int err;
    int i, n;

    /* Create a mesh client */
    err = mesh_create_client(&mc, client_config);
    if (err) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(1);
    }

    /* Create a mesh receiver connection */
    err = mesh_create_rx_connection(mc, &conn, conn_config);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_client;
    }

    /* Receive data loop */
    do {
        MeshBuffer *buf;

        /* Receive a buffer from the mesh */
        err = mesh_get_buffer(conn, &buf);
        if (err == MESH_ERR_CONNECTION_CLOSED) {
            printf("Connection closed\n");
            break;
        }
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        /* Process the received user data */
        get_user_video_frames(buf->payload_ptr, buf->payload_len);

        /* Release and put the buffer back to the mesh */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
    } while (1);

    /* Shutdown the connection */
    err = mesh_shutdown_connection(&conn);
    if (err)
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);

exit_delete_conn:
    /* Delete the media connection */
    mesh_delete_connection(&conn);

exit_delete_client:
    /* Delete the mesh client */
    mesh_delete_client(&mc);

    if (err)
        exit(1);
}
```
