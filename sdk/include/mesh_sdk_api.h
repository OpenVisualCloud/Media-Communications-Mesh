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

namespace mesh {

void * create_proxy_client(const std::string& endpoint, mesh::ClientContext *parent);
void   destroy_proxy_client(void *client);

void * create_proxy_conn(void *client, const mesh::ConnectionConfig& cfg);
void   destroy_proxy_conn(void *conn);

void * create_proxy_conn_zero_copy(void *client, const mesh::ConnectionConfig& cfg,
                                   const std::string& temporary_id);
int    configure_proxy_conn_zero_copy(void *conn_ptr);
void   destroy_proxy_conn_zero_copy(void *conn);

} // namespace mesh

#endif // MESH_SDK_API_H
