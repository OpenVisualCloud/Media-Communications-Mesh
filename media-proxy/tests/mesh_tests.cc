/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gtest/gtest.h>
#include "mesh/sync.h"

TEST(mesh_test, DataplaneAtomicUint64) {
    mesh::sync::DataplaneAtomicUint64 v;
    ASSERT_EQ(v.load(), 0);
    ASSERT_EQ(v.load_next(), 0);

    v.store_wait(123, std::chrono::milliseconds(100));
    ASSERT_EQ(v.load(), 123);
    ASSERT_EQ(v.load_next(), 123);

    // Simulate regular write access
    std::jthread th1([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        v.store_wait(567, std::chrono::milliseconds(5000));
    });

    // Simulate hotpath
    std::jthread th2([&]() {
        ASSERT_EQ(v.load_next(), 123);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ASSERT_EQ(v.load_next(), 567);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ASSERT_EQ(v.load_next(), 567);
    });

    // Simulate regular read access
    ASSERT_EQ(v.load(), 123);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ASSERT_EQ(v.load(), 123);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(v.load(), 567);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(v.load(), 567);
}
