# SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)

# MCM - Media Communications Mesh
project(MCM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-std=c++17 -lstdc++fs")
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

# Disable RPATH
set(CMAKE_SKIP_RPATH TRUE)

# setup components folders
set(MCM_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/media-proxy)
set(SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
set(SDK_INCLUDE_DIR ${SDK_DIR}/include)

# Setup output folder
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(${SDK_DIR})
add_subdirectory(${MP_DIR})

target_include_directories(media_proxy PUBLIC ${SDK_INCLUDE_DIR})
target_link_libraries(media_proxy PUBLIC mcm_dp)
