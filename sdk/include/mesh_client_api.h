/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MESH_CLIENT_API_H
#define MESH_CLIENT_API_H

#include "mcm_dp.h"
#include <string>

void * mesh_grpc_create_client();
void * mesh_grpc_create_client_json(std::string& addr, std::string& port);
void   mesh_grpc_destroy_client(void *client);
void * mesh_grpc_create_conn(void *client, mcm_conn_param *param);
void   mesh_grpc_destroy_conn(void *conn);

#endif // MESH_CLIENT_API_H
