# Mesh Data Plane SDK – Examples

## SMPTE ST2110-22 compressed video sender example

```c
int main(void)
{
    /* Default client configuration */
    MeshClientConfig client_config = { 0 };
    
    /* ST2110-XX configuration */
    MeshConfig_ST2110 conn_config = {
        .remote_ip_addr = "192.168.96.2",
        .remote_port = 9001,
        .transport = MESH_CONN_TRANSPORT_ST2110_22,
    };

    /* Video configuration */
    MeshConfig_Video payload_config = {
        .width = 1920,
        .height = 1080,
        .fps = 60,
        .pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422P10LE,
    };

    MeshConnection *conn;
    MeshClient *mc;
    int err;
    int i, n;

    /* Create a mesh client */
    err = mesh_create_client(&mc, &client_config);
    if (err) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(1);
    }

    /* Create a mesh connection */
    err = mesh_create_connection(mc, &conn);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_client;
    }

    /* Apply connection configuration */
    err = mesh_apply_connection_config_st2110(conn, &conn_config);
    if (err) {
        printf("Failed to apply SMPTE ST2110 configuration: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_conn;
    }

    /* Apply video payload configuration */
    err = mesh_apply_connection_config_video(conn, &video_config);
    if (err) {
        printf("Failed to apply video configuration: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_conn;
    }

    /* Establish a connection for sending data */
    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    if (err) {
        printf("Failed to establish connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_conn;
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
        put_user_video_frames(buf->data, buf->data_len);

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

## SMPTE ST2110-22 compressed video receiver example
```c
int main(void)
{
    /* Default client configuration */
    MeshClientConfig client_config = { 0 };
    
    /* ST2110-XX configuration */
    MeshConfig_ST2110 conn_config = {
        .remote_ip_addr = "192.168.96.1",
        .local_port = 9001,
        .transport = MESH_CONN_TRANSPORT_ST2110_22,
    };

    /* Video configuration */
    MeshConfig_Video payload_config = {
        .width = 1920,
        .height = 1080,
        .fps = 60,
        .pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422P10LE,
    };

    MeshConnection *conn;
    MeshClient *mc;
    int err;
    int i, n;

    /* Create a mesh client */
    err = mesh_create_client(&mc, &client_config);
    if (err) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(1);
    }

    /* Create a mesh connection */
    err = mesh_create_connection(mc, &conn);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_client;
    }

    /* Apply connection configuration */
    err = mesh_apply_connection_config_st2110(conn, &conn_config);
    if (err) {
        printf("Failed to apply SMPTE ST2110 configuration: %s (%d)\n",
               mesh_err2str(err), err);
        goto exit_delete_conn;
    }

    /* Apply video payload configuration */
    err = mesh_apply_connection_config_video(conn, &video_config);
    if (err) {
        printf("Failed to apply video configuration: %s (%d)\n",
               mesh_err2str(err), err);
        goto exit_delete_conn;
    }

    /* Establish a connection for receiving data */
    err = mesh_establish_connection(conn, MESH_CONN_KIND_RECEIVER);
    if (err) {
        printf("Failed to establish connection: %s (%d)\n",
               mesh_err2str(err), err);
        goto exit_delete_conn;
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
        get_user_video_frames(buf->data, buf->data_len);

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
