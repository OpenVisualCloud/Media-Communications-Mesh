/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _MCM_H_
#define _MCM_H_

#include <stdio.h>
#include "mesh_dp.h"

int mcm_init_client(MeshConnection **connection, MeshClient *client, const char *cfg);
int mcm_create_tx_connection(MeshConnection *connection, MeshClient *client, const char *cfg);
int mcm_create_rx_connection(MeshConnection *connection, MeshClient *client, const char *cfg);
int mcm_send_video_frames(MeshConnection *connection, const char *filename,
                          const char *json_conn_config);
int mcm_send_audio_packets(MeshConnection *connection, const char *filename,
                           const char *json_conn_config);
int mcm_send_blob_packets(MeshConnection *connection, const char *filename);
void read_data_in_loop(MeshConnection *connection, const char *filename);
void read_data_in_loop_with_interval(MeshConnection *connection, const char *filename, int save_interval_sec);
int is_root();

#endif /* _MCM_H_ */
