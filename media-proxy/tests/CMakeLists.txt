# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(McmMeifTests VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_C_STANDARD 11)

# Download fff.h
set(FFF_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/")

# Only download if not already present
if(NOT EXISTS "${FFF_DOWNLOAD_DIR}/fff.h")
    message(STATUS "Downloading fff.h...")

    file(DOWNLOAD
        "https://raw.githubusercontent.com/meekrosoft/fff/master/fff.h"
        "${FFF_DOWNLOAD_DIR}/fff.h"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
    )

    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download fff.h")
    endif()
endif()

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
target_compile_definitions(media_proxy_unit_tests PRIVATE UNIT_TESTS_ENABLED)

# Link the executable with gtest and gtest_main
target_link_libraries(media_proxy_unit_tests PRIVATE gtest gtest_main ${MP_LIB})
target_include_directories(media_proxy_unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/media-proxy/include
    # Include generated *.pb.h files
    ${CMAKE_BINARY_DIR}/media-proxy/generated
    ${FFF_DOWNLOAD_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add tests to CTest
add_test(NAME media_proxy_unit_tests COMMAND media_proxy_unit_tests)
