# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(McmDPSdk_Samples VERSION 0.1.0
        DESCRIPTION "MCM DP SDK Library Sample Code"
        LANGUAGES C
        )

set(CMAKE_C_STANDARD 11)

add_executable(sender_app sender_app.c)
target_include_directories(sender_app PRIVATE ../include)
target_link_libraries(sender_app PRIVATE mcm_dp_shared ${LIB_BSD})

add_executable(recver_app recver_app.c)
target_include_directories(recver_app PRIVATE ../include)
target_link_libraries(recver_app PRIVATE mcm_dp_shared ${LIB_BSD})
