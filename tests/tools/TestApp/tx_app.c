#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "Inc/input.h"
#include "Inc/mcm.h"

const char *client_cfg;
const char *conn_cfg;

int main(int argc, char **argv) {
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client_cfg.json> <connection_cfg.json> <path_to_input_file>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    char *client_cfg_file = argv[1];
    char *conn_cfg_file = argv[2];
    char *video_file = argv[3];

    MeshConnection *connection = NULL;
    MeshClient *client = NULL;

    printf("[TX] Launching TX app \n");
    printf("[TX] Reading client configuration... \n");
    client_cfg = parse_json_to_string(client_cfg_file);
    printf("[TX] Reading connection configuration... \n");
    conn_cfg = parse_json_to_string(conn_cfg_file);

    /* Initialize mcm client */
    int err = mesh_create_client_json(&client, client_cfg);
    if (err) {
        printf("[TX] Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(err);
    }

    /* Create mesh connection */
    err = mesh_create_tx_connection(client, &connection, conn_cfg);
    if (err) {
        printf("[TX] Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        mesh_delete_client(&client);
        exit(err);
    }

    /* Open file and send its contents */
    FILE *frames = fopen(video_file, "rb");
    mcm_send_video_frames(connection, client, frames);

    printf("[TX] Shuting down connection and client\n");
    mesh_delete_connection(&connection);
    mesh_delete_client(&client);
    printf("[TX] Shutdown completed. Exiting\n");
    free((char*)client_cfg);
    free((char*)conn_cfg);
    return 0;
}