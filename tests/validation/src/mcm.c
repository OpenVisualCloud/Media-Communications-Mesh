#include "mcm.h"
#include <stdio.h>

int mcm_init_client(mcm_ts* mcm, const char* cfg){
    int err = mesh_create_client(&(mcm->client), cfg);
    if (mesh_create_client(&(mcm->client), cfg)) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(1);
    }
    return err;
}

int mcm_create_tx_connection(mcm_ts* mcm, const char* cfg){
    int err = mesh_create_tx_connection(mcm->client, &(mcm->connection), cfg);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        mesh_delete_client(&(mcm->connection));
    }
    return err;
}


int mcm_create_rx_connection(mcm_ts* mcm, const char* cfg){
    int err = mesh_create_rx_connection(mc, &conn, conn_config);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
         mesh_delete_client(&(mcm->connection));
    }
    return err;
}


int mcm_send_video_frames(int num_of_frames, mcm_ts* mcm){
    int err = 0;
    /* Send data loop */
    for (i = 0; i < num_of_frames; i++) {
        MeshBuffer *buf;

        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(mcm->connection, &buf);
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        /* Fill the buffer with user data */
        put_user_video_frames(buf->data, buf->data_len);

        /* Send the buffer */
        err = mesh_put_buffer(&buf)
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
    }
    err = mesh_shutdown_connection(&(mcm->connection));
    if (err){
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    }
    return err;
}

int mcm_receive_video_frames(mcm_ts* mcm){
    int err = 0;
        /* Receive data loop */
    do {
        MeshBuffer *buf;

        /* Receive a buffer from the mesh */
        err = mesh_get_buffer(mcm->connection, &buf);
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
    err = mesh_shutdown_connection(&(mcm->connection));
    if (err){
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    }
    return err;
}