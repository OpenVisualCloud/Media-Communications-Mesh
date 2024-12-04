#ifndef _MCM_H_
#define _MCM_H

#include "mesh_dp.h"
#include "mcm_dp.h"

typedef enum protocol_te{
  MEMIF=0,
  NO_MEMIF
}protocol_te;
typedef enum payload_te{
  RDMA=0,
  ST2110_20,
  ST2110_22,
  ST2110_30,
  OTHER
}payload_te;

typedef struct mcm_config_ts {
    MeshConfig_Memif memif;
    MeshConfig_RDMA rdma;
    MeshConfig_ST2110 st2210;
}mcm_config_ts;
typedef struct mcm_ts{
    MeshClient *client;
    MeshConnection *conn;
    MeshBuffer *buf;

}mcm_ts;
int init_mcm(mcm_ts* mcm);


#endif /*_MCM_H_*/