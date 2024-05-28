#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

set -e

cd FFmpeg
cp -f ../mcm_* ./libavdevice/

make -j "$(nproc)"
sudo make install
cd ..

echo "FFmpeg MCM plugin build completed"
