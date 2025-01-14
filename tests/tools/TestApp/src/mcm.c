#include <stdlib.h>
#include <string.h>
#include "mcm.h"
#include "mcm_dp.h"
#include "mesh_dp.h"

/* PRIVATE */
void file_to_buffer(FILE *file, MeshBuffer* buf);
void buffer_to_file(FILE *file, MeshBuffer* buf);

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


int mcm_send_video_frame(mcm_ts* mcm, FILE* frame){
    int err = 0;
    MeshBuffer *buf;


    /* Ask the mesh to allocate a shared memory buffer for user data */
    err = mesh_get_buffer(mcm->connection, &buf);
    if (err) {
        printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
    }

    /* Fill the buffer with user data */
    //put_user_video_frames(buf->payload_ptr, buff->payload_len);
    //void * data = serialize_user_video_frame(frame);

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


int mcm_receive_video_frames(mcm_ts* mcm, FILE* frame){
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
    buffer_to_file(frame,buf);


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

void file_to_buffer(FILE *file, MeshBuffer* buf){
    if (file == NULL) {
        printf("Failed to serialize file: file is null \n");
        return;
    }

    /* Move the file pointer to the end of the file to determine the file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size > buf->payload_len) {
        printf("Failed to serialize file into buffer: file size exceed buffer size\n");
        return;
    }

    /* Move the file pointer back to the beginning of the file */
    fseek(file, 0, SEEK_SET);

    // Allocate memory for the byte array
    unsigned char *frame_buf = (unsigned char*)malloc(file_size);
    if (frame_buf == NULL) {
        printf("Failed to serialize file into buffer: failed to allocate memory\n");
        return;
    }

    // Read the file into the buffer
    fread(frame_buf, BYTE_SIZE, file_size, file);
    /* buf->payload_ptr is const type, it cannot be reassigned,
     * so frame_buf data needs to be copied under it's address
     */
    /* clear mesh buffer payload space */
    memset(buf->payload_ptr, FIRST_INDEX, buf->payload_len);

    /* copy frame_buf data into mesh buffer */
    memcpy(buf->payload_ptr, frame_buf, buf->payload_len);
    free(frame_buf);
}

void buffer_to_file(FILE *file, MeshBuffer* buf){
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }

    // Write the buffer to the file
    size_t written_size = fwrite(buf->payload_ptr, BYTE_SIZE, buf->payload_len, file);
    if (written_size != buf->payload_len) {
        perror("Failed to write buffer to file");
        fclose(file);
        return;
    }
    
    fclose(file);
}