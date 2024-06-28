# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)

project("McmDPSdk" C)
set(MCM_TARGET "libmcm_dp")
set(DESCRIPTION "MCM DP SDK Library")

set(CMAKE_C_STANDARD 11)
# extract version from git
execute_process(
  COMMAND git describe --long --tags
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE VER_RESULT
  OUTPUT_VARIABLE VER_VALUE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Check if succeeded if not use default one
if (${VER_RESULT} EQUAL 0)
    string(PREPEND VER_VALUE "v")
else ()
    set(VER_VALUE "v24.06-c660e83")
endif ()
message(STATUS "MCM library version: ${VER_VALUE}")

string(REGEX REPLACE "^v+([0-9]+)[-.]([0-9]+)[-.](.+)" "\\1;\\2;\\3" VER_VALUE ${VER_VALUE})
list(GET VER_VALUE 0 MCM_VER_MAJOR)
list(GET VER_VALUE 1 MCM_VER_MINOR)
list(GET VER_VALUE 2 MCM_VER_PATCH)
set(VERSION "${MCM_VER_MAJOR}.${MCM_VER_MINOR}.${MCM_VER_PATCH}")

# include modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# setup components folders
set(MCMDP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MCMDP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MEMIF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libmemif)
set(MCMDP_SAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/samples)

add_subdirectory(${MEMIF_DIR})

include(compile_options)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_IPATH ${CMAKE_INSTALL_PREFIX}/include)

aux_source_directory(${MCMDP_SRC_DIR} sdk_srcs)
add_library(mcm_dp SHARED ${sdk_srcs})
set_target_properties(mcm_dp PROPERTIES VERSION ${VERSION})
set_target_properties(mcm_dp PROPERTIES POSITION_INDEPENDENT_CODE ON)
# set_target_properties(mcm_dp PROPERTIES C_VISIBILITY_PRESET hidden)

target_include_directories(mcm_dp PUBLIC
    ${MCMDP_INCLUDE_DIR}
    ${MEMIF_DIR}/src)
target_link_libraries(mcm_dp PUBLIC memif)

add_subdirectory(${MCMDP_SAMPLE_DIR})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)

list(APPEND SDK_HEADERS
    include/mcm_dp.h
)

foreach(file ${SDK_HEADERS})
  get_filename_component(dir ${file} DIRECTORY)
    install(
	  FILES ${file}
	  DESTINATION ${CMAKE_INSTALL_IPATH}
	  COMPONENT ${MCM_TARGET}-dev
    )
endforeach()

set(LIB_PKG_CONFIG_EXTRA_CFLAGS "")
if (BUILD_SHARED_LIBS)
	target_compile_definitions(mcm_dp
        PUBLIC DEF_DLL
        PRIVATE DEF_BUILDING_SHARED_LIBS)
    set(LIB_PKG_CONFIG_EXTRA_CFLAGS " -DDEF_DLL")
endif()

configure_file(${MCM_TARGET}.pc.in ${CMAKE_BINARY_DIR}/${MCM_TARGET}.pc @ONLY)

# include(CMakePackageConfigHelpers)
install(TARGETS mcm_dp DESTINATION ${CMAKE_INSTALL_RPATH} COMPONENT ${MCM_TARGET})
install(FILES "${CMAKE_BINARY_DIR}/${MCM_TARGET}.pc" DESTINATION "${CMAKE_INSTALL_RPATH}/pkgconfig")
