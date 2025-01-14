#include "mcm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int mcm_init_client(mcm_ts* mcm, const char* cfg){
    int err = mesh_create_client_json(&(mcm->client), cfg);
    if (err) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(1);
    }
    return err;
}

int mcm_create_tx_connection(mcm_ts* mcm, const char* cfg){
    int err = mesh_create_tx_connection(mcm->client, &(mcm->connection), cfg);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        mesh_delete_client(&(mcm->client));
    }
    return err;
}


int mcm_create_rx_connection(mcm_ts* mcm, const char* cfg){
    int err = mesh_create_rx_connection(mcm->client, &(mcm->connection), cfg);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
         mesh_delete_client(&(mcm->client));
    }
    return err;
}


int mcm_send_video_frame(mcm_ts* mcm, const char* frame, int frame_len ){
    int err = 0;
    MeshBuffer *buf;

    /* Ask the mesh to allocate a shared memory buffer for user data */
    err = mesh_get_buffer(mcm->connection, &buf);
    if (err) {
        printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
    }

    /* Fill the buffer with user data */
    buf->payload_ptr = frame;
    put_user_video_frames(buf->payload_ptr, frame_len);

    /* Send the buffer */
    err = mesh_put_buffer(&buf);
    if (err) {
        printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
    }
    err = mesh_shutdown_connection(&(mcm->connection));
    if (err){
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    }
    return err;
}


int mcm_receive_video_frames(mcm_ts* mcm){
    int err = 0;
    MeshBuffer *buf;

    /* Receive a buffer from the mesh */
    err = mesh_get_buffer(mcm->connection, &buf);
    if (err == MESH_ERR_CONNECTION_CLOSED) {
        printf("Connection closed\n");
    }
    if (err) {
        printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
    }

    /* Process the received user data */
    get_user_video_frames(buf->payload_ptr, buf->payload_len);


    /* Release and put the buffer back to the mesh */
    err = mesh_put_buffer(&buf);
    if (err) {
        printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
    }

    /* Shutdown the connection */
    err = mesh_shutdown_connection(&(mcm->connection));
    if (err){
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    }
    return err;
}

int file_to_buffer(FILE* fp, MeshBuffer* buf, int frame_size)
{
    int ret = 0;

    assert(fp != NULL && buf != NULL);
    assert(buf->payload_len >= frame_size);

    if (fread(buf->payload_ptr, frame_size, 1, fp) < 1) {
        ret = -1;
    }
    return ret;
}