# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(McmMeifTests VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_C_STANDARD 11)

add_executable(memif_test_rx memif_test_rx.c)
target_include_directories(memif_test_rx PRIVATE ../include)
target_link_libraries(memif_test_rx PRIVATE mcm_dp)

add_executable(memif_test_tx memif_test_tx.c)
target_include_directories(memif_test_tx PRIVATE ../include)
target_link_libraries(memif_test_tx PRIVATE mcm_dp)

# Find source files for tests
file(GLOB TEST_SOURCES "*.cc")

set(MP_LIB media_proxy_lib)

# Add an executable for tests
add_executable(media_proxy_unit_tests ${TEST_SOURCES})

# Link the executable with gtest and gtest_main
target_link_libraries(media_proxy_unit_tests PRIVATE gtest gtest_main ${MP_LIB})
target_include_directories(media_proxy_unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/media-proxy/include
    # Include generated *.pb.h files
    ${CMAKE_BINARY_DIR}/media-proxy/generated
)

# Add tests to CTest
add_test(NAME media_proxy_unit_tests COMMAND media_proxy_unit_tests)
