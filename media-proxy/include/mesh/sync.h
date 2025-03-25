/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SYNC_H
#define SYNC_H

#include <atomic>
#include "concurrency.h"

namespace mesh::sync {

/**
 * DataplaneAtomicUint64
 * 
 * 64-bit unsigned integer custom atomic lock-free class designed to support
 * prioritized access levels:
 *   - Hotpath access    Method(s): load_next()
 *   - Regular access    Method(s): load(), store_wait()
 *
 * The hotpath access is the fastest possible lock-free approach to read
 * the 64-bit value. It is meant to be used in interrupts and callbacks
 * that happen when a new block of data appears on the network. The load_next()
 * method returns the 64-bit value. The hotpath access must be used in only
 * one thread. Multiple threads calling load_next() will lead to undetermined
 * behavior. The load_next() method should be called twice in the hotpath:
 * before processing and after processing.
 * 
 * The regular access is a normal mutex-protected way to read and write the
 * 64-bit value. The store_wait() method is used to write a new value to the
 * class instance. It blocks for some time until the hotpath flow calls
 * load_next(), or a timeout occurs. The timeout interval should be chosen to be
 * at least twice longer than the longest hotpath processing duration.
 * The load() method returns the 64-bit value. Both methods can be called from
 * multiple threads.
 * 
 * Example:
 * DataplaneAtomicUint64 is used as a storage of a class pointer
 * 
 * // Declaration
 * sync::DataplaneAtomicUint64 myclass_ptr;
 * 
 * // Hotpath (single dedicated thread)
 * ...
 * auto ptr = (MyClass *)myclass_ptr.load_next();
 * if (ptr)
 *     ptr->do_something();
 * myclass_ptr.load_next();
 * ...
 * 
 * // Regular thread (one or many)
 * ...
 * auto ptr = (MyClass *)myclass_ptr.load();
 * if (ptr)
 *     ptr->do_something_else();
 * myclass_ptr.store_wait((uint64_t)new_ptr, std::chrono::milliseconds(100));
 * ...
 */
class DataplaneAtomicUint64 {
public:
    uint64_t load();
    void store_wait(uint64_t new_value, std::chrono::milliseconds timeout_ms);

    uint64_t load_next();

private:
    std::atomic<uint64_t> current = 0;
    std::atomic<uint64_t> next = 0;
    std::mutex mx;
};

} // namespace mesh::sync

#endif // SYNC_H
