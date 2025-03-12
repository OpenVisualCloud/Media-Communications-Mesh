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
set(AGENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/control-plane-agent)

# Setup output folder
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(${SDK_DIR})
add_subdirectory(${MP_DIR})
add_subdirectory(${AGENT_DIR})

# Get current version: Git tag and hash
execute_process(
  COMMAND git describe --tags --always
  OUTPUT_VARIABLE GIT_TAG
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
  COMMAND git rev-parse --short HEAD
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Generate a version header file
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/mcm-version.h.in
  ${CMAKE_CURRENT_SOURCE_DIR}/mcm-version.h
  @ONLY
)

# Generate a version.go file
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/mcm-version.go.in
  ${AGENT_DIR}/internal/mcm-version.go
  @ONLY
)

target_include_directories(media_proxy_lib PUBLIC ${SDK_INCLUDE_DIR})
option(BUILD_UNIT_TESTS "Build and enable unit tests" ON)
if (BUILD_UNIT_TESTS)
  # Fetch google test dependencies
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/35d0c365609296fa4730d62057c487e3cfa030ff.zip
  )
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  add_subdirectory(${MP_TESTS_DIR})
  add_subdirectory(${SDK_TESTS_DIR})
  target_link_libraries(media_proxy_lib PUBLIC gtest gmock gtest_main)
else()
  target_link_libraries(media_proxy_lib PUBLIC)
endif()
