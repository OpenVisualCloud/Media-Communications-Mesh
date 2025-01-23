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
    MeshBuffer *buf;
    if (file == NULL) {
        printf("Failed to serialize video: file is null \n");
        return 1;
    }

    err = mesh_get_buffer(connection, &buf);
    if (err) {
        printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
    }


    unsigned char frame_num = 0;
    size_t read_size = 1;
    while(1){

        if (frame_num > 0){
            /* Ask the mesh to allocate a shared memory buffer for user data */
            err = mesh_get_buffer(connection, &buf);
            if (err) {
                printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            }
        }
        read_size = fread(buf->payload_ptr, 1, buf->payload_len , file);
        if(read_size == 0) {
            break;
        }

        /* Send the buffer */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
        }

        usleep(40000);
    };
    printf("[TXAPP DEBUG]closing mesh connection\n");
    err = mesh_shutdown_connection(connection);
    if (err){
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);
    }
    return err;

}

void read_data_in_loop(MeshConnection *connection,const char* filename){
    int timeout = MESH_TIMEOUT_INFINITE;
    int frame = 0;
    int err = 0;
    MeshBuffer *buf = NULL;
       while(1){
        FILE *out = fopen(filename, "a");
        /* Set loop's  error*/
        err = 0;
        // mcm_receive_video_frames(connection, client, out, frame);
        if (frame){
            timeout = 1000;
        }

        /* Receive a buffer from the mesh */
        err = mesh_get_buffer_timeout(connection, &buf, timeout);
        if (err == MESH_ERR_CONN_CLOSED) {
            printf("Connection closed\n");
            break;
        }
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
        /* Process the received user data */
        buffer_to_file(out, buf);

        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
        printf("Frame: %d\n", ++frame);
        fclose(out);
    } 
}


void buffer_to_file(FILE *file, MeshBuffer* buf){
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }
    // Write the buffer to the file
    unsigned int *temp_buf = buf->payload_ptr;
    printf("[RXAPP DEBUG ] saving buffer data to a file\n");
    fwrite(buf->payload_ptr,buf->payload_len, 1, file);
}

int is_root() {
    return (geteuid() == 0) ? 1 : 0; 
}