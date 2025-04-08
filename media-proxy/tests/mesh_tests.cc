/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gtest/gtest.h>
#include "mesh/sync.h"

TEST(mesh_test, DataplaneAtomicPtr) {
    mesh::sync::DataplaneAtomicPtr ptr;
    ASSERT_EQ(ptr.load(), nullptr);
    ASSERT_EQ(ptr.load_next_lock(), nullptr);
    ptr.unlock();

    ptr.store_wait((void *)0x400);
    ASSERT_EQ(ptr.load(), (void *)0x400);
    ASSERT_EQ(ptr.load_next_lock(), (void *)0x400);
    ptr.unlock();

    // Simulate regular write access
    std::jthread th1([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ptr.store_wait((void *)0x500);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ASSERT_EQ(ptr.load(), (void *)0x500);
    });

    // Simulate hotpath
    std::jthread th2([&]() {
        ASSERT_EQ(ptr.load_next_lock(), (void *)0x400);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ptr.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ASSERT_EQ(ptr.load_next_lock(), (void *)0x500);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ptr.unlock();
    });

    ASSERT_EQ(ptr.load(), (void *)0x400);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ASSERT_EQ(ptr.load(), (void *)0x400);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(ptr.load(), (void *)0x500);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(ptr.load(), (void *)0x500);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(ptr.load(), (void *)0x500);
}
