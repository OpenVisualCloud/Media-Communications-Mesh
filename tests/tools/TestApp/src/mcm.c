#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mcm.h"
#include "mcm_dp.h"
#include "mesh_dp.h"

/* PRIVATE */
void buffer_to_file(FILE *file, MeshBuffer* buf);

int mcm_send_video_frame(MeshConnection *connection, MeshClient *client, FILE* file){


    // fread() in while loop, 
    // every time get one buffer chunk until fread >0 
    int err = 0;
    MeshBuffer *buf;

    printf("Getting a buffer\n");
    /* Ask the mesh to allocate a shared memory buffer for user data */
    err = mesh_get_buffer(connection, &buf);
    if (err) {
        printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
    }

    if (err) {
        printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
    }
    uint32_t buf_size = buf->payload_len;

    if (file == NULL) {
        printf("Failed to serialize video: file is null \n");
        return 1;
    }

    unsigned char frame_num = 0;
    size_t read_size = 1;
    while(1){

        if (frame_num > 0){
            printf("Getting a buffer\n");
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
        unsigned int *print_buf = buf->payload_ptr;
        printf("buf->payload_ptr directly after fread: first bytes from file_buf: %04x, %04x, %04x, %04x, %04x \n",
         print_buf[frame_num],
         print_buf[3*(frame_num+1)],
         print_buf[4*(frame_num+1)],
         print_buf[5*(frame_num+1)],
         print_buf[6*(frame_num+1)]);

        printf("sending %d  frame \n", ++frame_num);
        /* Send the buffer */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
        }

        usleep(40000);
    };

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
    unsigned int *temp_buf = buf->payload_ptr;
    printf("buffer_to_file: first bytes from mesh buf: %04x, %04x, %04x, %04x, %04x \n",
         temp_buf[0],
         temp_buf[1],
         temp_buf[2],
         temp_buf[3],
         temp_buf[4]);
    fwrite(buf->payload_ptr,buf->payload_len, 1, file);
    // if(fputs(buf->payload_ptr, file) == EOF) {
    //    perror("Failed to write buffer to file");
    //    fclose(file);
    // }
}

int is_root() {
    return (geteuid() == 0) ? 1 : 0; 
}