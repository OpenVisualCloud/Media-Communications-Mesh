# SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

# Minimum CMake required
cmake_minimum_required(VERSION 3.16)

project(MediaProxy VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)

if(CMAKE_CROSSCOMPILING)
  find_program(_PROTOBUF_PROTOC protoc)
else()
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP gRPC::grpc++)
if(CMAKE_CROSSCOMPILING)
  find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

# Enabled by default
option(ENABLE_ZERO_COPY "Enable zero-copy mode to avoid memory copying between Media Proxy and IMTL" ON)
IF(ENABLE_ZERO_COPY)
	ADD_DEFINITIONS(-DZERO_COPY)
ENDIF(ENABLE_ZERO_COPY)

set(proto_name controller)
set(target media_proxy)
set(MTL_LIB mtl)
set(MEMIF_LIB memif)

# Get proto files
get_filename_component(proto "./protos/${proto_name}.proto" ABSOLUTE)
get_filename_component(proto_dir "${proto}" DIRECTORY)

# Generate source files
set(proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.cc")
set(proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.h")
set(grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.cc")
set(grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.h")

aux_source_directory(./src proxy_srcs)
list(REMOVE_ITEM proxy_srcs "./src/media_proxy.cc")

add_definitions(-DHAVE_MEMFD_CREATE)

add_custom_command(
    OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${proto_dir}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${proto}"
    DEPENDS "${proto}"
)

set(CMAKE_INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_CONFIG_PATH ${CMAKE_INSTALL_PREFIX}/etc)
ADD_DEFINITIONS(-DIMTL_CONFIG_PATH=\"${CMAKE_INSTALL_PREFIX}/etc/imtl.json\")

# Include generated *.pb.h files
include_directories(${CMAKE_CURRENT_BINARY_DIR} ./include)

add_executable(media_proxy ${proto_srcs} ${grpc_srcs} ${proxy_srcs} ./src/media_proxy.cc)
target_link_libraries(media_proxy PRIVATE m ${MTL_LIB} ${_REFLECTION} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} ${MEMIF_LIB})

install(TARGETS media_proxy DESTINATION ${CMAKE_INSTALL_PATH} COMPONENT media_proxy)
install(FILES imtl.json DESTINATION ${CMAKE_CONFIG_PATH} COMPONENT config)
