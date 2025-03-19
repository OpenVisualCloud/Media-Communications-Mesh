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

#define SHUTDOWN_REQUESTED 1


char *client_cfg;
char *conn_cfg;
MeshConnection *connection = NULL;
MeshClient *client = NULL;
struct sigaction sa_int;
struct sigaction sa_term;
int shutdown = 0;

void sig_handler(int sig);
void setup_signal_handler(struct sigaction *sa, void (*handler)(int),int sig);
int is_shutdown_requested();

int main(int argc, char **argv) {
    setup_signal_handler(&sa_int, sig_handler, SIGINT);
    setup_signal_handler(&sa_term, sig_handler, SIGTERM);
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
        goto safe_exit;
    }

    /* Open file and send its contents in loop*/
    while(1){
        err = mcm_send_video_frames(connection, video_file, is_shutdown_requested);
        if ( shutdown == SHUTDOWN_REQUESTED ) {
            goto safe_exit;
        }
    }
safe_exit:
    LOG("[TX] shut down request, dropping connection to media-proxy...");
    LOG("[TX] Shuting down connection");
    if (connection) {
        mesh_delete_connection(&connection);
    }
    LOG("[TX] Shuting down client");
    if (client) {
        mesh_delete_client(&client);
    }
    free(client_cfg);
    free(conn_cfg);
    return err;
}
int is_shutdown_requested() {
    return shutdown;
}

void sig_handler(int sig) {
        shutdown = SHUTDOWN_REQUESTED;
}

void setup_signal_handler(struct sigaction *sa, void (*handler)(int),int sig) {
    sa->sa_handler = handler;
    sigemptyset(&(sa->sa_mask));
    sa->sa_flags = 0;
    sigaction(sig, sa, NULL);
}

