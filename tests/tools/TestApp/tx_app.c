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
struct sigaction sa;

void handle_sigint(int sig);
void setup_signal_handler();

int main(int argc, char **argv) {
    setup_signal_handler();
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

    LOG("[TX] Launching TX app");
    
    LOG("[TX] Reading client configuration...");
    client_cfg = parse_json_to_string(client_cfg_file);
    LOG("[TX] Reading connection configuration...");
    conn_cfg = parse_json_to_string(conn_cfg_file);

    /* Initialize mcm client */
    int err = mesh_create_client_json(&client, client_cfg);
    if (err) {
        LOG("[TX] Failed to create mesh client: %s (%d)", mesh_err2str(err), err);
        goto safe_exit;
    }

    /* Create mesh connection */
    err = mesh_create_tx_connection(client, &connection, conn_cfg);
    if (err) {
        LOG("[TX] Failed to create connection: %s (%d)", mesh_err2str(err), err);
        mesh_delete_client(&client);
        goto safe_exit;
    }

    /* Open file and send its contents */

    err = mcm_send_video_frames(connection, video_file);
    LOG("[TX] Shuting down connection and client");
    mesh_delete_connection(&connection);
    mesh_delete_client(&client);
    LOG("[TX] Shutdown completed. Exiting");

safe_exit:
    free(client_cfg);
    free(conn_cfg);
    return err;
}

void handle_sigint(int sig) {
    LOG("[TX] SIGINT interrupt, dropping connection to media-proxy...");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    exit(EXIT_SUCCESS);
}

void setup_signal_handler() {
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

