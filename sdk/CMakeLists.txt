# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)

project("McmDPSdk" C CXX)
set(MCM_TARGET "libmcm_dp")
set(DESCRIPTION "MCM DP SDK Library")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
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
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../protos)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_subdirectory(${MEMIF_DIR})

include(compile_options)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_IPATH ${CMAKE_INSTALL_PREFIX}/include)

# Find Protobuf and gRPC
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

# Ensure that the gRPC plugin is available
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN_EXECUTABLE)
    message(FATAL_ERROR "grpc_cpp_plugin not found")
endif()

# Define the .proto files
set(PROTO_FILES
    ${PROTO_DIR}/mediaproxy.proto
    ${PROTO_DIR}/sdk.proto
)

# Generate the protobuf and gRPC source files
set(PROTO_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

if(CMAKE_CROSSCOMPILING)
  find_program(Protobuf_PROTOC_EXECUTABLE protoc)
else()
  set(Protobuf_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
endif()

foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME_WE)
    set(proto_srcs "${PROTO_SRC_DIR}/${proto_name}.pb.cc")
    set(proto_hdrs "${PROTO_SRC_DIR}/${proto_name}.pb.h")
    set(grpc_srcs "${PROTO_SRC_DIR}/${proto_name}.grpc.pb.cc")
    set(grpc_hdrs "${PROTO_SRC_DIR}/${proto_name}.grpc.pb.h")

    add_custom_command(
        OUTPUT ${proto_srcs} ${proto_hdrs} ${grpc_srcs} ${grpc_hdrs}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --proto_path=${PROTO_DIR}
             --grpc_out=${PROTO_SRC_DIR}
             --cpp_out=${PROTO_SRC_DIR}
             --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
             ${proto_file}
        DEPENDS ${proto_file}
        COMMENT "Generating Protobuf and gRPC files for ${proto_file}"
    )

    list(APPEND PROTO_SRCS ${proto_srcs})
    list(APPEND PROTO_HDRS ${proto_hdrs})
    list(APPEND GRPC_SRCS ${grpc_srcs})
    list(APPEND GRPC_HDRS ${grpc_hdrs})
endforeach()

aux_source_directory(${MCMDP_SRC_DIR} sdk_srcs)
add_library(mcm_dp SHARED ${sdk_srcs} ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS})
set_target_properties(mcm_dp PROPERTIES VERSION ${VERSION})
set_target_properties(mcm_dp PROPERTIES POSITION_INDEPENDENT_CODE ON)
# set_target_properties(mcm_dp PROPERTIES C_VISIBILITY_PRESET hidden)

target_include_directories(mcm_dp PUBLIC
    ${MCMDP_INCLUDE_DIR}
    ${MEMIF_DIR}/src
    ${Protobuf_INCLUDE_DIRS}
    ${gRPC_INCLUDE_DIRS}
    ${PROTO_SRC_DIR}  # Include the directory where the generated files are located
)
target_link_libraries(mcm_dp PUBLIC memif gRPC::grpc++ protobuf::libprotobuf)

add_subdirectory(${MCMDP_SAMPLE_DIR})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)

list(APPEND SDK_HEADERS
    # include/mcm_dp.h
    include/mesh_dp.h
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
