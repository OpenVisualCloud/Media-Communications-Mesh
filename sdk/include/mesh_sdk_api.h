/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MESH_SDK_API_H
#define MESH_SDK_API_H

#include "mcm_dp.h"
#include <string>
#include "mesh_conn.h"

void * mesh_grpc_create_client();
void * mesh_grpc_create_client_json(const std::string& endpoint);
void   mesh_grpc_destroy_client(void *client);
void * mesh_grpc_create_conn(void *client, mcm_conn_param *param);
void * mesh_grpc_create_conn_json(void *client, const mesh::ConnectionJsonConfig& cfg);
void   mesh_grpc_destroy_conn(void *conn);

#endif // MESH_SDK_API_H
