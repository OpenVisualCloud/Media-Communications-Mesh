/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "Inc/input.h"
#include "Inc/mcm.h"
#include "Inc/misc.h"

char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;

int main(int argc, char *argv[]) {
    setup_sig_int();
    if (!is_root()) {
        fprintf(stderr, "This program must be run as root. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <client_cfg.json> <connection_cfg.json> <path_to_output_file>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    char *client_cfg_file = argv[1];
    char *conn_cfg_file = argv[2];
    char *out_filename = argv[3];

    LOG("[RX] Launching RX App");
    LOG("[RX] Reading client configuration...");
    client_cfg = input_parse_file_to_string(client_cfg_file);
    LOG("[RX] Reading connection configuration...");
    conn_cfg = input_parse_file_to_string(conn_cfg_file);

    /* Initialize mcm client */
    int err = mesh_create_client_json(&client, client_cfg);
    if (err) {
        LOG("[RX] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[RX] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }
    LOG("[RX] Waiting for packets...");
    read_data_in_loop(connection, out_filename);

safe_exit:
    LOG("[RX] dropping connection to media-proxy...");
    if (connection) {
        LOG("[RX] Shuting down connection");
        mesh_delete_connection(&connection);
    }
    if (client) {
        LOG("[RX] Shuting down client");
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    return err;
}