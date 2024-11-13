/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_BUF_H
#define MESH_BUF_H

#include "mesh_dp.h"
#include "mcm_dp.h"
#include "mesh_conn.h"

namespace mesh {

/**
 * Mesh connection buffer structure
 */
class BufferContext {
public:
    BufferContext(ConnectionContext *conn);

    int dequeue(int timeout_ms);
    int enqueue(int timeout_ms);

    /**
     * NOTE: The __public structure is directly mapped in the memory to the
     * MeshBuffer structure, which is publicly accessible to the user.
     * Therefore, the __public structure _MUST_ be placed first here.
     */
    MeshBuffer __public = {};

    /**
     * NOTE: All declarations below this point are hidden from the user.
     */
    mcm_buffer *buf = nullptr;
};

} // namespace mesh

#endif // MESH_BUF_H
