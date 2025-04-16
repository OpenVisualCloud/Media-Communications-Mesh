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
 * DataplaneAtomicPtr
 * 
 * Custom atomic lock-free class to store a void memory pointer designed to
 * support prioritized access levels:
 *   - Hotpath access    Method(s): load_next_lock(), unlock()
 *   - Regular access    Method(s): load(), store_wait()
 *
 * The hotpath access is the fastest possible lock-free approach to read
 * the memory pointer value. It is meant to be used in interrupts and callbacks
 * that happen when a new block of data appears on the network. The critical
 * section begins with an invokation of the load_next_lock() method that returns
 * the pointer value and locks the write access. To exit the critical section,
 * the unlock() method must be called, which unlocks the write access.
 * 
 * The hotpath access must be used in only one thread. Multiple threads calling
 * load_next_lock() and/or lock() will lead to undetermined behavior. 
 * 
 * The regular access is a normal mutex-protected way to read and write the
 * pointer value. The store_wait() method is used to write a new value to the
 * class instance. It blocks infinitely until the hotpath unlocks the write
 * access. The load() method returns the 64-bit value. Both methods can be
 * called from multiple threads.
 * 
 * Example:
 * 
 * // Declaration
 * sync::DataplaneAtomicPtr myclass_ptr;
 * 
 * // Hotpath (single dedicated thread)
 * ...
 * auto ptr = (MyClass *)myclass_ptr.load_next_lock();
 * if (ptr)
 *     ptr->do_something();
 * myclass_ptr.unlock();
 * ...
 * 
 * // Regular thread (one or many)
 * ...
 * auto ptr = (MyClass *)myclass_ptr.load();
 * if (ptr)
 *     ptr->do_something_else();
 * myclass_ptr.store_wait((void *)new_ptr);
 * ...
 */
class DataplaneAtomicPtr {
    public:
        void * load();
        void store_wait(void *new_ptr);

        void * load_next_lock();
        void unlock();

    private:
        std::atomic<uint64_t> current = 0;
        std::atomic<uint64_t> next = 0;
        std::mutex mx;
    };
    
} // namespace mesh::sync

#endif // SYNC_H
