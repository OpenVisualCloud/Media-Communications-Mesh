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

# Find source files for general tests
file(GLOB MEDIA_PROXY_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/libfabric_cq_tests.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/libfabric_dev_tests.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/libfabric_ep_tests.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/libfabric_mocks.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/libfabric_mr_tests.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/proxy_context_tests.cc"
)

# Find source files for RDMA-specific tests
file(GLOB RDMA__RX_TX_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/conn_rdma_rx_tests.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/conn_rdma_tx_tests.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/conn_rdma_test_mocks.cc"
)

# Find source files for physical-RDMA-specific tests
file(GLOB PHYS_RDMA_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/conn_rdma_real_ep_test.cc"
)

# Find source files for physical-RDMA-specific tests
file(GLOB RDMA_BASE_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/conn_rdma_tests.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/conn_rdma_test_mocks.cc"
)

set(MP_LIB media_proxy_lib)

# Add an executable for general tests
add_executable(media_proxy_unit_tests ${MEDIA_PROXY_TEST_SOURCES})
target_compile_definitions(media_proxy_unit_tests PRIVATE UNIT_TESTS_ENABLED)
target_link_libraries(media_proxy_unit_tests PRIVATE gtest gtest_main ${MP_LIB})
target_include_directories(media_proxy_unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/media-proxy/include
    ${CMAKE_BINARY_DIR}/media-proxy/generated
    ${FFF_DOWNLOAD_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add an executable for RDMA-specific tests
add_executable(conn_rdma_rx_tx_unit_tests ${RDMA__RX_TX_TEST_SOURCES})
target_link_libraries(conn_rdma_rx_tx_unit_tests PRIVATE gmock gmock_main gtest gtest_main ${MP_LIB})
target_include_directories(conn_rdma_rx_tx_unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/media-proxy/include
    # Include generated *.pb.h files
    ${CMAKE_BINARY_DIR}/media-proxy/generated
    ${FFF_DOWNLOAD_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add an executable for RDMA-base-specific tests
add_executable(conn_rdma_base_unit_tests ${RDMA_BASE_TEST_SOURCES})
target_compile_definitions(conn_rdma_base_unit_tests PRIVATE UNIT_TESTS_ENABLED)
target_link_libraries(conn_rdma_base_unit_tests PRIVATE gmock gmock_main gtest gtest_main ${MP_LIB})
target_include_directories(conn_rdma_base_unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/media-proxy/include
    # Include generated *.pb.h files
    ${CMAKE_BINARY_DIR}/media-proxy/generated
    ${FFF_DOWNLOAD_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add an executable for physical-RDMA-specific tests
add_executable(conn_rdma_real_ep_test ${PHYS_RDMA_TEST_SOURCES})
target_link_libraries(conn_rdma_real_ep_test PRIVATE gtest gtest_main ${MP_LIB})
target_include_directories(conn_rdma_real_ep_test PUBLIC
    ${CMAKE_SOURCE_DIR}/media-proxy/include
    # Include generated *.pb.h files
    ${CMAKE_BINARY_DIR}/media-proxy/generated
    ${FFF_DOWNLOAD_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add tests to CTest
add_test(NAME conn_rdma_rx_tx_unit_tests COMMAND conn_rdma_rx_tx_unit_tests)
add_test(NAME media_proxy_unit_tests COMMAND media_proxy_unit_tests)
add_test(NAME conn_rdma_real_ep_test COMMAND conn_rdma_real_ep_test)
add_test(NAME conn_rdma_base_unit_tests COMMAND conn_rdma_base_unit_tests)
