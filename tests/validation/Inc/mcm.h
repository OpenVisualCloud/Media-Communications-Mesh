#ifndef _MCM_H_
#define _MCM_H_
#include "mcm_mock.h"

typedef struct mcm_ts{
    MeshConnection *connection;
    MeshClient *client;
}mcm_ts;


int mcm_init_client(mcm_ts* mcm, const char* cfg);
int mcm_create_tx_connection(mcm_ts* mcm, const char* cfg);
int mcm_create_rx_connection(mcm_ts* mcm, const char* cfg);
int mcm_send_video_frames(mcm_ts* mcm, int num_of_frames);
int mcm_receive_video_frames(mcm_ts* mcm);

#endif /* _MCM_H_*/