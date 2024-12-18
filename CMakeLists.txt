# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)

# MCM - Media Communications Mesh
project(MCM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-std=c++20 -lstdc++fs")
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

# Disable RPATH
set(CMAKE_SKIP_RPATH TRUE)

# setup components folders
set(MCM_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/media-proxy)
set(MP_TESTS_DIR ${MP_DIR}/tests)
set(SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
set(SDK_TESTS_DIR ${SDK_DIR}/tests)
set(SDK_INCLUDE_DIR ${SDK_DIR}/include)
set(TESTS_VAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/TestApp)

# Setup output folder
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(${SDK_DIR})
add_subdirectory(${MP_DIR})


# Fetch google test dependencies
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/35d0c365609296fa4730d62057c487e3cfa030ff.zip
)
FetchContent_MakeAvailable(googletest)

target_include_directories(media_proxy_lib PUBLIC ${SDK_INCLUDE_DIR})
option(BUILD_UNIT_TESTS "Build and enable unit tests" ON)
if (BUILD_UNIT_TESTS)
    enable_testing()
    add_subdirectory(${MP_TESTS_DIR})
    add_subdirectory(${SDK_TESTS_DIR})
    add_subdirectory(${TESTS_VAL_DIR})
    target_link_libraries(media_proxy_lib PUBLIC mcm_dp gtest gmock gtest_main)
else()
    target_link_libraries(media_proxy_lib PUBLIC mcm_dp)
endif()