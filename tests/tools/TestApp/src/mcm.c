#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mcm.h"
#include "mcm_dp.h"
#include "mesh_dp.h"

/* PRIVATE */
void buffer_to_file(FILE *file, MeshBuffer* buf);

int mcm_send_video_frame(MeshConnection *connection, MeshClient *client, FILE* file){
    int err = 0;
    uint32_t frame_size = connection->buf_size;

    if (file == NULL) {
        printf("Failed to serialize video: file is null \n");
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    unsigned char *file_buf = (unsigned char*)malloc(frame_size);
    fread(file_buf, BYTE_SIZE, frame_size, file);

    // err = mesh_get_buffer(connection, &buf);
    int num_of_frames = file_size / frame_size;
    printf("file_size: %li\n", file_size);
    printf("frame size: %u\n", frame_size);
    if (num_of_frames == 0 ){
        num_of_frames = 1;
    }
    printf("%d frames to send\n", num_of_frames);
    fseek(file, 0, SEEK_SET);

    for(int i = 0 ; i < 500; i++){
        MeshBuffer *buf;

        printf("Getting a buffer\n");
        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(connection, &buf);
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
        }
        printf("Buffer fetched %lu\n", buf->payload_len);
        /* get next chunk data, move pointer to the next chunk start */
        // unsigned char *temp_buf = file_buf + (i * buf->payload_len);
        unsigned char *temp_buf = file_buf;

        /* clear mesh buffer payload space */
        // memset(buf->payload_ptr, FIRST_INDEX, buf->payload_len);
        /* copy frame_buf data into mesh buffer */
        memcpy(buf->payload_ptr, temp_buf, buf->payload_len);
        printf("sending %d  frame \n", i+1);
        /* Send the buffer */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
        }
        printf("Before usleep\n");
        usleep(40000);
    }
    free(file_buf);
    fclose(file);

    err = mesh_shutdown_connection(connection);
    if (err){
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    }
    return err;
}

int mcm_receive_video_frames(MeshConnection *connection, MeshClient *client, FILE* file, int frame){
    int err = 0;
    MeshBuffer *buf = NULL;

    /* Receive a buffer from the mesh */
    err = mesh_get_buffer(connection, &buf);
    if (err == MESH_ERR_CONN_CLOSED) {
        printf("Connection closed\n");
    }
    if (err) {
        printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
    }
    printf("Received buf of %li B length\n", buf->payload_len);
    /* Process the received user data */
    buffer_to_file(file, buf);
    printf("---\n");


    /* Release and put the buffer back to the mesh */
    err = mesh_put_buffer(&buf);
    if (err) {
        printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
    }
    printf("Released the buffer buf of %li B length\n", buf->payload_len);

    /* Shutdown the connection */
    // err = mesh_shutdown_connection(connection);
    // if (err){
    //     printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    // }
    // printf("The connection has been shut down\n");
    // return err;
    printf("Frame: %i", frame);
    frame += 1;
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
    printf("Received 1 video frame of %li B and saved it into the file\n", buf->payload_len);
}
