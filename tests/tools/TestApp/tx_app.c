#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "Inc/input.h"
#include "Inc/mcm.h"




const char* client_cfg;
const char* conn_cfg;

int main(int argc, char** argv){
    if(!is_root()){
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client_cfg.json> <connection_cfg.json> <abs path>/file|frame>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    char* client_cfg_file = argv[1];
    char* conn_cfg_file = argv[2];
    char* frame_file = argv[3];

    MeshConnection *connection = NULL;
    MeshClient *client = NULL;

    printf("launching TX app \n");
    printf("reading client configuration... \n");  
    client_cfg = parse_json_to_string(client_cfg_file);
    printf("reading connection configuration... \n");
    conn_cfg = parse_json_to_string(conn_cfg_file);

    /* Initialize mcm client */
    int err = mesh_create_client_json(&client, client_cfg);
    if (err) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        return err;
    }

    /* Create mesh connection */
    err = mesh_create_tx_connection(client, &connection, conn_cfg);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        mesh_delete_client(&client);
        return err;
    }

    /* Do not remove the sleep(), required for proper alignment */
    // sleep(5);

    /* Open file and send its contents */
    FILE *frame = fopen(frame_file, "rb");
    mcm_send_video_frame(connection, client, frame);

    mesh_delete_connection(&connection);
    mesh_delete_client(&client);

    return 0;
}