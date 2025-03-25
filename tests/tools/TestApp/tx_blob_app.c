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
 
 int main(int argc, char **argv) {
     parse_cli_commands(argc, argv);
     setup_sig_int();
     if (!is_root()) {
         fprintf(stderr, "This program must be run as root. Exiting.\n");
         exit(EXIT_FAILURE);
     }
     if (argc < 4) {
         fprintf(stderr, "Usage: %s <client_cfg.json> <connection_cfg.json> <path_to_input_file>\n",
                 argv[0]);
         exit(EXIT_FAILURE);
     }
 
     char *client_cfg_file = argv[1];
     char *conn_cfg_file = argv[2];
     char *video_file = argv[3];
 
     LOG("[TX] Launching TX app");
     
     LOG("[TX] Reading client configuration...");
     client_cfg = input_parse_file_to_string(client_cfg_file);
     LOG("[TX] Reading connection configuration...");
     conn_cfg = input_parse_file_to_string(conn_cfg_file);
 
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
     if (input_loop == -1) {
        LOG("[TX] sending video frames inf times");
        while(1){
                /* Open file and send its contents in loop*/
                err = mcm_send_blob_packets(connection, video_file);
                if (err) {
                    LOG("[TX] Failed to send blob: %s (%d)", mesh_err2str(err), err);
                }
            }
    } else if (input_loop >0) {
        LOG("[TX] sending video frames %d times", input_loop);
        for (int i = 0; i < input_loop; i++){
            /* Open file and send its contents in loop*/
            err = mcm_send_blob_packets(connection, video_file);
            if (err) {
                LOG("[TX] Failed to send blob: %s (%d)", mesh_err2str(err), err);
            }
        }
     } else {
        LOG("[TX] sending video frames 1 time");
        /* Open file and send its contents in loop*/
        err = mcm_send_blob_packets(connection, video_file);
        if (err) {
            LOG("[TX] Failed to send blob: %s (%d)", mesh_err2str(err), err);
        }
    }
 safe_exit:
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
 