# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)

# Project name
project(ControlPlaneAgent)

# Set the Go compiler
find_program(GO_EXECUTABLE NAMES go)

if(NOT GO_EXECUTABLE)
    message(FATAL_ERROR "Go compiler not found. Please install Go.")
endif()

# Find the protoc compiler
find_program(PROTOC_EXECUTABLE NAMES protoc)

if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc compiler not found. Please install Protocol Buffers.")
endif()

# Define the paths to the protoc-gen-go and protoc-gen-go-grpc plugins
set(PROTOC_GEN_GO_EXECUTABLE "$ENV{HOME}/go/bin/protoc-gen-go")
set(PROTOC_GEN_GO_GRPC_EXECUTABLE "$ENV{HOME}/go/bin/protoc-gen-go-grpc")

if(NOT EXISTS ${PROTOC_GEN_GO_EXECUTABLE})
    message(FATAL_ERROR "protoc-gen-go plugin not found. Please install the Go Protocol Buffers plugin.")
endif()

if(NOT EXISTS ${PROTOC_GEN_GO_GRPC_EXECUTABLE})
    message(FATAL_ERROR "protoc-gen-go-grpc plugin not found. Please install the Go gRPC plugin.")
endif()

# Define the source directory and the main Go file
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmd/mesh-agent")
set(MAIN_GO_FILE "${SOURCE_DIR}/mesh-agent.go")
set(CMAKE_INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/bin)

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

# Generate a version.go file
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/internal/mcm-version.go.in
  ${CMAKE_CURRENT_SOURCE_DIR}/internal/mcm-version.go
  @ONLY
)

# Define the proto files and the output directories for generated code
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../protos/mediaproxy.proto")
set(GENERATED_DIR_MEDIAPROXY "${CMAKE_CURRENT_SOURCE_DIR}/api/proxy/proto/mediaproxy")
set(GENERATED_DIR_SDK "${CMAKE_CURRENT_SOURCE_DIR}/api/proxy/proto/sdk")

# Define the output directory for the executable
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../_build/mcm/bin")

# Create the output directories if they don't exist
file(MAKE_DIRECTORY ${OUTPUT_DIR})
file(MAKE_DIRECTORY ${GENERATED_DIR_MEDIAPROXY})
file(MAKE_DIRECTORY ${GENERATED_DIR_SDK})

# Define the output executable name
set(EXECUTABLE_NAME "mesh-agent")

# Add a custom target to generate gRPC code from the proto files
add_custom_target(generate_grpc
    COMMAND ${PROTOC_EXECUTABLE}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/../protos
            --go_out=paths=source_relative:${GENERATED_DIR_MEDIAPROXY}
            --go-grpc_out=paths=source_relative:${GENERATED_DIR_MEDIAPROXY}
            --plugin=protoc-gen-go=${PROTOC_GEN_GO_EXECUTABLE}
            --plugin=protoc-gen-go-grpc=${PROTOC_GEN_GO_GRPC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../protos/mediaproxy.proto
    COMMAND ${PROTOC_EXECUTABLE}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/../protos
            --go_out=paths=source_relative:${GENERATED_DIR_SDK}
            --go-grpc_out=paths=source_relative:${GENERATED_DIR_SDK}
            --plugin=protoc-gen-go=${PROTOC_GEN_GO_EXECUTABLE}
            --plugin=protoc-gen-go-grpc=${PROTOC_GEN_GO_GRPC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../protos/conn-config.proto
    COMMENT "Generating gRPC code from proto files..."
)

# Add a custom target to run all unit tests
add_custom_target(gotest
    COMMAND ${GO_EXECUTABLE} test -timeout=30s ./...
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Go unit tests..."
    DEPENDS generate_grpc
)

# Add a custom target to build the Go project
add_custom_target(build ALL
    COMMAND ${GO_EXECUTABLE} build -o ${OUTPUT_DIR}/${EXECUTABLE_NAME} ${MAIN_GO_FILE}
    WORKING_DIRECTORY ${SOURCE_DIR}
    COMMENT "Building Go project..."
    DEPENDS generate_grpc gotest
)

install(PROGRAMS ${OUTPUT_DIR}/${EXECUTABLE_NAME} DESTINATION ${CMAKE_INSTALL_PATH})
