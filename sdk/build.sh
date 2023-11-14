# SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

#!/bin/bash

# Set build type. ("Debug" or "Release")
BUILD_TYPE="${BUILD_TYPE:-Release}"

cmake -B out -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" .
cmake --build out -j

# Install
sudo cmake --install out
