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
