/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "sync.h"

namespace mesh::sync {

static const uint64_t STATE_UNLOCKED = 0;
static const uint64_t STATE_LOCKED   = 2;
static const uint64_t FLAG_NEW_VALUE = 1;

void * DataplaneAtomicPtr::load()
{
    return (void *)current.load(std::memory_order_acquire);
}

void DataplaneAtomicPtr::store_wait(void *new_ptr)
{
    std::lock_guard<std::mutex> lk(mx);

    uint64_t new_value = (uint64_t)new_ptr;

    auto prev = next.exchange(new_value | FLAG_NEW_VALUE, std::memory_order_acq_rel);
    if (prev == STATE_LOCKED) {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            if (current.load(std::memory_order_acquire) == new_value)
                return;
        }
    }

    current.store(new_value, std::memory_order_release);
    return;
}

void * DataplaneAtomicPtr::load_next_lock()
{
    uint64_t next_value = next.exchange(STATE_LOCKED, std::memory_order_acq_rel);
    uint64_t current_value;

    if (next_value & FLAG_NEW_VALUE) {
        current_value = next_value & ~FLAG_NEW_VALUE;
        current.store(current_value, std::memory_order_release);
    } else {
        current_value = current.load(std::memory_order_acquire);
    }

    return (void *)current_value;
}

void DataplaneAtomicPtr::unlock()
{
    uint64_t next_value = next.exchange(STATE_UNLOCKED, std::memory_order_acq_rel);

    if (next_value & FLAG_NEW_VALUE)
        current.store(next_value & ~FLAG_NEW_VALUE, std::memory_order_release);
}

} // namespace mesh::sync
