#include "mcm.h"

void error_delete_conn(mcm_ts* mcm){
    mesh_delete_connection(&(mcm->conn));
}
void error_delete_client(mcm_ts* mcm){
    mesh_delete_client(&(mcm->client));
    exit(-1);
}

int mcm_init(mcm_ts* mcm, protocol_te protocol, payload_te  payload, mcm_config_ts cfg){
    if (mesh_create_client(&(mcm->client), NULL)) {
        printf("Failed to create a mesh client: %s (%d)\n",
               mesh_err2str(err), err);
        exit(-1);
    }

    if (mesh_create_connection(mcm->client, &(mcm->conn))) {
        printf("Failed to create a mesh connection: %s (%d)\n",
               mesh_err2str(err), err);
        error_delete_client(mcm);
        
    }

    if(protocol == MEMIF){
          if (mesh_apply_connection_config_memif(mcm->conn, &(cfg->memif))) {
          printf("Failed to apply memif configuration: %s (%d)\n",
               mesh_err2str(err), err);
          error_delete_conn(mcm);
          return 1
        }
    } else /* non memeif protocol */ {
      if(payload == RDMA){
          if (mesh_apply_connection_config_rdma(conn, &(cfg->rdma))) {
            printf("Failed to apply RDMA configuration: %s (%d)\n",
              mesh_err2str(err), err);
            error_delete_conn(mcm);
            return 1
            }
      } else /* st2210 transport type */{
            /* set up */
            void set+up_st2210(){

            }
            switch (payload){             
              case ST2110_20:
                cfg->st2210.transport = MESH_CONN_TRANSPORT_ST2110_20;
                if (mesh_apply_connection_config_st2110(conn, &(cfg->st2210))) {
                  printf("Failed to apply SMPTE ST2110 configuration: %s (%d)\n",
                      mesh_err2str(err), err);
                  error_delete_conn(mcm);
                }
                break;
              case ST2110_22:
                cfg->st2210.transport = MESH_CONN_TRANSPORT_ST2110_22;
                if (mesh_apply_connection_config_st2110(conn, &(cfg->st2210))) {
                  printf("Failed to apply SMPTE ST2110 configuration: %s (%d)\n",
                      mesh_err2str(err), err);
                  error_delete_conn(mcm);
                }                
                break;
              case ST2110_30:
                cfg->st2210.transport = MESH_CONN_TRANSPORT_ST2110_30;
                if (mesh_apply_connection_config_st2110(conn, &(cfg->st2210))) {
                  printf("Failed to apply SMPTE ST2110 configuration: %s (%d)\n",
                      mesh_err2str(err), err);
                  error_delete_conn(mcm);
                }
                break;
              default:
                printf("Unknown SMPTE ST2110 transport type: %s\n", payload_type);
                error_delete_conn(mcm);
                return 1
            }
      }




}
