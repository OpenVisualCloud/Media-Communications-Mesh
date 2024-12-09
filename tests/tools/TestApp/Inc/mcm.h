#ifndef _MCM_H_
#define _MCM_H_
#include "mcm_mock.h"

#define DUMMY_LEN 1
typedef struct mcm_ts{
    MeshConnection *connection;
    MeshClient *client;
}mcm_ts;


int mcm_init_client(mcm_ts* mcm, const char* cfg);
int mcm_create_tx_connection(mcm_ts* mcm, const char* cfg);
int mcm_create_rx_connection(mcm_ts* mcm, const char* cfg);
int mcm_send_video_frame(mcm_ts* mcm, const char* frame, int frame_len);
int mcm_receive_video_frames(mcm_ts* mcm);
int file_to_buffer(FILE* fp, MeshBuffer* buf, int frame_size);

#endif /* _MCM_H_*/