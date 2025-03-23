/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "sync.h"

namespace mesh::sync {

uint64_t DataplaneAtomicUint64::load()
{
    return current.load(std::memory_order_acquire);
}

void DataplaneAtomicUint64::store_wait(uint64_t new_value,
                                       std::chrono::milliseconds timeout_ms)
{
    std::lock_guard<std::mutex> lk(mx);

    next.store(new_value, std::memory_order_release);

    auto ctx = context::WithTimeout(context::Background(),
                                    std::chrono::milliseconds(timeout_ms));

    for (;;) {
        thread::Sleep(ctx, std::chrono::milliseconds(5));

        if (ctx.cancelled()) {
            current.store(new_value, std::memory_order_release);
            return;
        }

        if (current.load(std::memory_order_acquire) == new_value)
            return;
    }
}

uint64_t DataplaneAtomicUint64::load_next()
{
    uint64_t next_value = next.load(std::memory_order_acquire);
    current.store(next_value, std::memory_order_release);
    return next_value;
}

} // namespace mesh::sync
