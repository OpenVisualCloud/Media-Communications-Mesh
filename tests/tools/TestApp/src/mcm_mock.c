#include "mcm_mock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define FIFO_NAME "/tmp/my_fifo"
#define BUFFER_SIZE 128

int fd;
char buffer[BUFFER_SIZE];

const char* mesh_err2str(int err) {return "error";}
int mesh_create_client(MeshClient **client,const char *config_json){ return 0;}
void mesh_delete_client(MeshClient **client){}
int mesh_shutdown_connection(MeshConnection **conn){ 
  close(fd);
  return 0;}

int mesh_create_tx_connection(MeshClient *client, MeshConnection **conn, const char *config_json){
    // Create the named pipe (FIFO)
    mkfifo(FIFO_NAME, 0666);

    // Open the FIFO for writing
    fd = open(FIFO_NAME, O_WRONLY);
    if (fd == -1) {
        perror("cannot open FIFO channel");
        return 1;
    }
  return 0;
}
int mesh_create_rx_connection(MeshClient *client, MeshConnection **conn, const char *config_json){
    fd = open(FIFO_NAME, O_RDONLY);
    if (fd == -1) {
        perror("cannot open FIFO channel");
        return 1;
    }  
  return 0;
}

int mesh_get_buffer(MeshConnection *conn, MeshBuffer **buf){return 0;}
int mesh_put_buffer(MeshBuffer **buf){return 0;}

void put_user_video_frames(void* ptr, const size_t len){
  printf("sending: %s\n", (char*)ptr);
  write(fd, ptr, len + 1);
}
int get_user_video_frames(void* ptr, const size_t len){
  ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
  ptr = buffer;
  printf("Received: %s\n", buffer);
}
