/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BUF_H
#define BUF_H

#include <cstdint>
#include <cstddef>

namespace mesh::connection {

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

} // namespace mesh::connection

#endif // BUF_H
