# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

"""
Log validation constants for FFmpeg validation.
"""

# Required ordered log phrases for FFmpeg Rx validation
FFMPEG_RX_REQUIRED_LOG_PHRASES = [
    '[DEBU] JSON client config:',
    '[INFO] Media Communications Mesh SDK version',
    '[DEBU] JSON conn config:',
    '[INFO] gRPC: connection created',
    'INFO - Create memif socket.',
    'INFO - Create memif interface.',
    'INFO - memif connected!',
    '[INFO] gRPC: connection active'
]

# Required ordered log phrases for FFmpeg Tx validation
FFMPEG_TX_REQUIRED_LOG_PHRASES = [
    '[DEBU] JSON client config:',
    '[INFO] Media Communications Mesh SDK version',
    '[DEBU] JSON conn config:',
    '[DEBU] BUF PARTS'
]

# Common error keywords to look for in logs
FFMPEG_ERROR_KEYWORDS = [
    "ERROR",
    "FATAL",
    "exception",
    "segfault",
    "core dumped",
    "failed",
    "FAIL",
    "[error]"
]
