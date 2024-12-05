#include "mcm_mock.h"


const char* mesh_err2str(int err) {return "error";}
int mesh_create_client(MeshClient **client,const char *config_json){ return 1;}
void mesh_delete_client(MeshClient **client){}
int mesh_shutdown_connection(MeshConnection **conn){ return 1;}

int mesh_create_tx_connection(MeshClient *client, MeshConnection **conn, const char *config_json){return 1;}
int mesh_create_rx_connection(MeshClient *client, MeshConnection **conn, const char *config_json){return 1;}

int mesh_get_buffer(MeshConnection *conn, MeshBuffer **buf){return 1;}
int mesh_put_buffer(MeshBuffer **buf){return 1;}

void put_user_video_frames(void * const ptr, const size_t len){}
int get_user_video_frames(void * const ptr, const size_t len){return 1;}
