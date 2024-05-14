# SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)

project("McmDPSdk" C)
set(VERSION "0.1.0")
set(DESCRIPTION "MCM DP SDK Library")

#set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_STANDARD 99)

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
#set_target_properties(mcm_dp PROPERTIES C_VISIBILITY_PRESET hidden)

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
	COMPONENT libmcm_dp-dev
      )
endforeach()


set(LIB_PKG_CONFIG_EXTRA_CFLAGS "")
if (BUILD_SHARED_LIBS)
	target_compile_definitions(mcm_dp
        PUBLIC DEF_DLL
        PRIVATE DEF_BUILDING_SHARED_LIBS)
    set(LIB_PKG_CONFIG_EXTRA_CFLAGS " -DDEF_DLL")
endif()
configure_file(libmcm_dp.pc.in libmcm_dp.pc @ONLY)

install(TARGETS mcm_dp DESTINATION ${CMAKE_INSTALL_RPATH} COMPONENT libmcm_dp)
install(FILES ${CMAKE_BINARY_DIR}/sdk/libmcm_dp.pc DESTINATION "${CMAKE_INSTALL_RPATH}/pkgconfig")

