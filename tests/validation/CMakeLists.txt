# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(McmDPSdk_Validation VERSION 0.1.0
        DESCRIPTION "MCM DP SDK Library Validation Code"
        LANGUAGES C
        )

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lz -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -lz -fPIC")

add_executable(recver_val recver_val.c common.c)
target_include_directories(recver_val PRIVATE ../include)
target_link_libraries(recver_val PRIVATE mcm_dp)

add_executable(sender_val sender_val.c common.c)
target_include_directories(sender_val PRIVATE ../include)
target_link_libraries(sender_val PRIVATE mcm_dp)

install(TARGETS recver_val sender_val RUNTIME COMPONENT Runtime)
