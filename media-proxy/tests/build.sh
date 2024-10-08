#!/bin/bash -x

# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

# compile tests
# -DMEMIF_DBG_SHM
# -DMEMIF_DBG
gcc -DHAVE_MEMFD_CREATE -Wno-unused-result -I.. -I../include -I../../sdk/3rdparty/libmemif/src -Iinclude -g -Og memif_test_tx.c ../../sdk/3rdparty/libmemif/src/*.c -lpthread -o memif_test_tx
gcc -DHAVE_MEMFD_CREATE -Wno-unused-result -I.. -I../include -I../../sdk/3rdparty/libmemif/src -Iinclude -g -Og memif_test_rx.c ../../sdk/3rdparty/libmemif/src/*.c -lpthread -o memif_test_rx
# gcc -DHAVE_MEMFD_CREATE -Wno-unused-result -I.. -I../../dependency/vpp/extras/libmemif/src -Iinclude -g -Og ffmpeg_videosrc.c ../../dependency/vpp/extras/libmemif/src/*.c -lst_dpdk -lpthread -lavformat -lavcodec -lavutil -lavfilter -o ffmpeg_videosrc
# gcc -DHAVE_MEMFD_CREATE -Wno-unused-result -I.. -I../../dependency/vpp/extras/libmemif/src -Iinclude -g -Og filtering_video.c ../../dependency/vpp/extras/libmemif/src/*.c -lpthread -lavformat -lavcodec -lavutil -lavfilter -o filtering_video
