#ifndef _MCM_H_
#define _MCM_H_

#include <stdio.h>
#include "mcm_dp.h"
#include "mesh_dp.h"

#define BYTE_SIZE 1
#define FIRST_INDEX 0
typedef struct mcm_ts{
    MeshConnection *connection;
    MeshClient *client;
}mcm_ts;


int mcm_init_client(mcm_ts* mcm, const char* cfg);
int mcm_create_tx_connection(mcm_ts* mcm, const char* cfg);
int mcm_create_rx_connection(mcm_ts* mcm, const char* cfg);
int mcm_send_video_frame(mcm_ts* mcm, FILE* frame);
int mcm_receive_video_frames(mcm_ts* mcm);

#endif /* _MCM_H_*/