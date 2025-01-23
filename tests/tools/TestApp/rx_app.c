#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "Inc/input.h"
#include "Inc/mcm.h"

const char *client_cfg;
const char *conn_cfg;

int main(int argc, char *argv[]) {
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client_cfg.json> <connection_cfg.json> <path_to_output_file>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    char *client_cfg_file = argv[1];
    char *conn_cfg_file = argv[2];
    char *out_filename = argv[3];

    MeshConnection *connection = NULL;
    MeshClient *client = NULL;

    printf("[RX] Launching RX App \n");
    printf("[RX] Reading client configuration... \n");
    client_cfg = parse_json_to_string(client_cfg_file);
    printf("[RX] Reading connection configuration... \n");
    conn_cfg = parse_json_to_string(conn_cfg_file);

    /* Initialize mcm client */
    int err = mesh_create_client_json(&client, client_cfg);
    if (err) {
        printf("[RX] Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(err);
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        printf("[RX] Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        mesh_delete_client(&client);
        exit(err);
    }
    printf("[RX] Waiting for frames... \n");
    read_data_in_loop(connection, out_filename);
    printf("[RX] Shuting down connection and client\n");
    mesh_delete_connection(&connection);
    mesh_delete_client(&client);
    printf("[RX] Shutdown completed exiting\n");

    return 0;
}