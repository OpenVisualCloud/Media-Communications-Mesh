/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_BUF_MEMIF_H
#define MESH_BUF_MEMIF_H

#include "mesh_buf.h"

namespace mesh {

/**
 * Mesh connection memif buffer class
 */
class MemifBufferContext : public BufferContext {
public:
    explicit MemifBufferContext(ConnectionContext *conn) : BufferContext(conn) {}

    mcm_buffer *buf = nullptr;
};

} // namespace mesh

#endif // MESH_BUF_MEMIF_H
