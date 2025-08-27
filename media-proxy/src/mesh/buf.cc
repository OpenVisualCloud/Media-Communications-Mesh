/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "buf.h"

namespace mesh::connection {

size_t BufferPartitions::total_size() const {
    return payload.size + metadata.size + sysdata.size;
}

} // namespace mesh::connection
