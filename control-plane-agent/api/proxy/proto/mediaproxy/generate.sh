#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -eo pipefail
PROTOC_PATH="$HOME/.local/bin/protoc"
PROTO_FILENAME="mediaproxy.proto"

"$PROTOC_PATH" --go_out=. --go_opt=paths=source_relative --go-grpc_out=. --go-grpc_opt=paths=source_relative "$PROTO_FILENAME" && echo Generated successfully
