/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_BUF_H
#define MESH_BUF_H

#include "mesh_dp.h"

namespace mesh {

class ConnectionContext;

/**
 * Mesh connection base buffer class
 */
class BufferContext {
public:
    BufferContext(ConnectionContext *conn);

    int put(int timeout_ms);
    int setPayloadLen(size_t size);
    int setMetadataLen(size_t size);

    /**
     * NOTE: The __public structure is directly mapped in the memory to the
     * MeshBuffer structure, which is publicly accessible to the user.
     * Therefore, the __public structure _MUST_ be placed first here.
     */
    MeshBuffer __public = {};
};

/**
 * Buffer partition definition structure
 */
class BufferPartition {
public:
    uint32_t size;
    uint32_t offset;
};

/**
 * Buffer partitioning definition structure
 */
class BufferPartitions {
public:
    BufferPartition payload;
    BufferPartition metadata;
    BufferPartition sysdata;

    size_t total_size() const;
};

/**
 * System data structure transmitted within every buffer
 */
class BufferSysData {
public:
    int64_t timestamp_ms;
    uint32_t seq;
    uint32_t payload_len;
    uint32_t metadata_len;
};
    
} // namespace mesh

#endif // MESH_BUF_H
