#include "mcm_mock.h"
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define COMMON_SPACE "/tmp/MCM_MOCK"
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
  return 0;
}
int mesh_create_rx_connection(MeshClient *client, MeshConnection **conn, const char *config_json){
    fd = open(COMMON_SPACE, O_RDONLY);
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

    char *file = (char*)ptr;
    char cwd[2048];
    char src_path[2048];
    char dest_path[2048];

    // Get the current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // Construct the source file path
    snprintf(src_path, sizeof(src_path) + 2, "%s/%s", cwd, file);
    // Construct the destination file path
    snprintf(dest_path, sizeof(dest_path) + 2, "%s/%s", COMMON_SPACE, basename(file));
    // Move the file
    if (rename(file, dest_path) != 0) {
        perror("rename");
        exit(EXIT_FAILURE);
    }

    // Print the message
    printf("file sent\n");
}
int get_user_video_frames(void* ptr, const size_t len){
     char cwd[2048];
    char src_path[2048];
    char dest_path[2048];
    DIR *dir;
    struct dirent *entry;

    // Get the current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // Open the directory /tmp/my_dir
    dir = opendir(COMMON_SPACE);
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    // Iterate over each entry in the directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip the "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the source file path
        snprintf(src_path, sizeof(src_path) + 2, "%s/%s", COMMON_SPACE, entry->d_name);

        // Construct the destination file path
        snprintf(dest_path, sizeof(dest_path) + 2, "%s/%s", cwd, entry->d_name);
        // Move the file
        if (rename(src_path, dest_path) != 0) {
            perror("rename");
            closedir(dir);
            exit(EXIT_FAILURE);
        }
        // Close the directory
        closedir(dir);
        // Print the message
        printf("File received\n");

    }
    closedir(dir);
    return 0;
}
