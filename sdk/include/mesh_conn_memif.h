/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_CONN_MEMIF_H
#define MESH_CONN_MEMIF_H

#include "mesh_conn.h"

namespace mesh {

class MemifConnectionContext : public ConnectionContext {
public:
    explicit MemifConnectionContext(ClientContext *parent) : ConnectionContext(parent) {}
    ~MemifConnectionContext() override {}

    int establish() override;
    int shutdown() override;
    int get_buffer(MeshBuffer **buf, int timeout_ms) override;
    int put_buffer(MeshBuffer *buf, int timeout_ms) override;
};

} // namespace mesh

#endif // MESH_CONN_MEMIF_H
