#include <stdlib.h>
#include <string.h>
#include "mcm.h"
#include "mcm_dp.h"
#include "mesh_dp.h"

/* PRIVATE */
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


int mcm_send_video_frame(mcm_ts* mcm, FILE* file){
    int err = 0;
    MeshBuffer *buf;

    if (file == NULL) {
        printf("Failed to serialize video: file is null \n");
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    unsigned char *file_buf = (unsigned char*)malloc(file_size);
    fread(file_buf, BYTE_SIZE, file_size, file);

    err = mesh_get_buffer(mcm->connection, &buf);
    int num_of_frames = file_size / buf->payload_len;
    printf("file_size: %li\n", file_size);
    printf("payload_len: %li\n", buf->payload_len);
    if (num_of_frames == 0 ){
        num_of_frames = 1;
    }
    printf("%d frames to send\n", num_of_frames);
    fseek(file, 0, SEEK_SET);

    for(int i = 0 ; i < num_of_frames; i++){
        
        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(mcm->connection, &buf);
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
        }
        /* get next chunk data, move pointer to the next chunk start */
        unsigned char *temp_buf = file_buf + (i * buf->payload_len);

        /* clear mesh buffer payload space */
        memset(buf->payload_ptr, FIRST_INDEX, buf->payload_len);
        /* copy frame_buf data into mesh buffer */
        memcpy(buf->payload_ptr, temp_buf, buf->payload_len);
        printf("sending %d  frame \n", i+1);
        /* Send the buffer */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
        }
    }
    free(file_buf);
    fclose(file);

    err = mesh_shutdown_connection(mcm->connection);
    if (err){
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    }
    return err;
}

int mcm_receive_video_frames(mcm_ts* mcm, FILE* file){
    int err = 0;
    MeshBuffer *buf;

    /* Receive a buffer from the mesh */
    err = mesh_get_buffer(mcm->connection, &buf);
    if (err == MESH_ERR_CONN_CLOSED) {
        printf("Connection closed\n");
    }
    if (err) {
        printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
    }
    /* Process the received user data */
    buffer_to_file(file,buf);


    /* Release and put the buffer back to the mesh */
    err = mesh_put_buffer(&buf);
    if (err) {
        printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
    }

    /* Shutdown the connection */
    err = mesh_shutdown_connection(mcm->connection);
    if (err){
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    }
    return err;
}

void buffer_to_file(FILE *file, MeshBuffer* buf){
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }
    // Write the buffer to the file
    size_t written_size = fwrite(buf->payload_ptr, BYTE_SIZE, buf->payload_len, file);
    if(fputs(buf->payload_ptr, file) == EOF) {
       perror("Failed to write buffer to file");
       fclose(file);
    }
}
