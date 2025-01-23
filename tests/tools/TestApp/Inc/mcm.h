#ifndef _MCM_H_
#define _MCM_H_

#include <stdio.h>
#include "mesh_dp.h"

#define BYTE_SIZE 1
#define FIRST_INDEX 0

int mcm_init_client(MeshConnection **connection, MeshClient *client, const char *cfg);
int mcm_create_tx_connection(MeshConnection *connection, MeshClient *client, const char *cfg);
int mcm_create_rx_connection(MeshConnection *connection, MeshClient *client, const char *cfg);
int mcm_send_video_frame(MeshConnection *connection, MeshClient *client, FILE *file);
void buffer_to_file(FILE *file, MeshBuffer *buf);
void read_data_in_loop(MeshConnection *connection, const char *filename);
int is_root();
#endif /* _MCM_H_*/