# SDK API Examples — Media Communications Mesh

## SMPTE ST 2110-20 uncompressed video receiver example
```c
int main(void)
{
    /* Default client configuration */
    const char *client_config = R"(
      {
        "apiVersion": "v1",
        "apiConnectionString": "Server=127.0.0.1; Port=8002"
      }
    )";

    // Receiver connection configuration
    const char *conn_config = R"(
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
        "payload": {
          "video": {
            "width": 1920,
            "height": 1080,
            "fps": 60.0,
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
    for (;;) {
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
    }

    /* Shutdown the connection */
    err = mesh_shutdown_connection(&conn);
    if (err)
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);

    /* Delete the media connection */
    mesh_delete_connection(&conn);

exit_delete_client:
    /* Delete the mesh client */
    mesh_delete_client(&mc);

    if (err)
        exit(1);
}
```

## SMPTE ST 2110-20 uncompressed video sender example

```c
int main(void)
{
    /* Default client configuration */
    const char *client_config = R"(
      {
        "apiVersion": "v1",
        "apiConnectionString": "Server=127.0.0.1; Port=8002"
      }
    )";

    // Transmitter connection configuration
    const char *conn_config = R"(
      {
        "connection": {
          "st2110": {
            "transport": "st2110-20",
            "ipAddr": "224.0.0.1",
            "port": 9002,
            "pacing": "narrow",
            "payloadType": 112,
            "transportPixelFormat": "yuv422p10rfc4175"
          },
        },
        "payload": {
          "video": {
            "width": 1920,
            "height": 1080,
            "fps": 60.0,
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

    /* Create a mesh transmitter connection */
    err = mesh_create_tx_connection(mc, &conn, conn_config);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_client;
    }

    /* 1000 video frames to be sent */
    n = 1000;

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

        /* Put some time interval between the frames */
        enforce_user_frame_send_interval();

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

    /* Delete the media connection */
    mesh_delete_connection(&conn);

exit_delete_client:
    /* Delete the mesh client */
    mesh_delete_client(&mc);

    if (err)
        exit(1);
}
```


<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
