#include <stdio.h>
#include "Inc/input.h"
#include "Inc/mcm.h"
#include <unistd.h>
#include <stdlib.h>

const char *client_cfg;
const char *conn_cfg;

int main(int argc, char *argv[]) {
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <client_cfg.json> <connection_cfg.json> <abs path>/file|frame>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    char *client_cfg_file = argv[1];
    char *conn_cfg_file = argv[2];
    char *out_filename = argv[3];

    MeshConnection *connection = NULL;
    MeshClient *client = NULL;

    printf("launching RX App \n");
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
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        mesh_delete_client(&client);
        return err;
    }
    printf("[RXAPP INFO ] waiting for frames... \n");
    read_data_in_loop(connection, out_filename);
    printf("[RXAPP INFO ] shuting down connection and client\n");
    mesh_delete_connection(&connection);
    mesh_delete_client(&client);
    printf("[RXAPP INFO ] shutdown completed exiting\n");

    return 0;
}