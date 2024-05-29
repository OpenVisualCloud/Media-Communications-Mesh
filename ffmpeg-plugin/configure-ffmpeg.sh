#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -e

cd FFmpeg
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
pkg-config --exists --print-errors libmcm_dp

# copy source files to allow the configure tool to find them
#cp -f ../mcm_* ./libavdevice/

./configure --enable-shared --enable-mcm
cd ..

echo "FFmpeg MCM plugin configuration completed"
