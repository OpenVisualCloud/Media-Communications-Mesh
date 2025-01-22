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


    // err = mesh_get_buffer(connection, &buf);
    int num_of_frames = file_size / frame_size;
    printf("file_size: %li\n", file_size);
    printf("frame size: %u\n", frame_size);
    if (num_of_frames == 0 ){
        num_of_frames = 1;
    }

    // unsigned char *file_buf = (unsigned char*)malloc(frame_size);
    unsigned char *file_buf = (unsigned char*)calloc(num_of_frames, frame_size);
    fread(file_buf, BYTE_SIZE, frame_size, file);

    printf("%d frames to send\n", num_of_frames);
    fseek(file, 0, SEEK_SET);

    unsigned char *temp_buf = file_buf;
    for(int i = 0 ; i < num_of_frames; i++){
        MeshBuffer *buf;

        printf("Getting a buffer\n");
        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(connection, &buf);

        temp_buf = file_buf + (i * buf->payload_len);
        printf("temp_buf = %p\n", temp_buf);
        printf("file_buf = %p\n", file_buf);

        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
        }
        printf("Buffer fetched %lu\n", buf->payload_len);

        /* copy frame_buf data into mesh buffer */
        printf("Before memcpy\n");
        memcpy(buf->payload_ptr, temp_buf, buf->payload_len);
        printf("After memcpy\n");
        printf("sending %d  frame \n", i+1);

        /* Send the buffer */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
        }

        usleep(40000);
    }
    free(file_buf);

    err = mesh_shutdown_connection(connection);
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
    if(fputs(buf->payload_ptr, file) == EOF) {
       perror("Failed to write buffer to file");
       fclose(file);
    }
    printf("Received 1 video frame of %li B and saved it into the file\n", buf->payload_len);
}
