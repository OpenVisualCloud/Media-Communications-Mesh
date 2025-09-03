# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

"""
Common constants for log validation used by both rxtxapp and ffmpeg validation.
"""

# Required ordered log phrases for Rx validation
RX_REQUIRED_LOG_PHRASES = [
    "[RX] Reading client configuration",
    "[RX] Reading connection configuration",
    "[DEBU] JSON client config:",
    "[INFO] Media Communications Mesh SDK version",
    "[DEBU] JSON conn config:",
    "[RX] Fetched mesh data buffer",
    "[RX] Saving buffer data to a file",
    "[RX] Done reading the data",
    "[RX] dropping connection to media-proxy",
    "INFO - memif disconnected!",
]

# Optional phrases for Rx validation (may not appear if process is terminated)
RX_OPTIONAL_LOG_PHRASES = [
    "[RX] Done reading the data",
    "[RX] dropping connection to media-proxy",
    "INFO - memif disconnected!",
]

# Required ordered log phrases for Tx validation
TX_REQUIRED_LOG_PHRASES = [
    "[TX] Reading client configuration",
    "[TX] Reading connection configuration",
    "[DEBU] JSON client config:",
    "[INFO] Media Communications Mesh SDK version",
    "[DEBU] JSON conn config:",
    "[INFO] gRPC: connection created",
    "INFO - Create memif socket.",
    "INFO - Create memif interface.",
]

# Common error keywords to look for in logs
RX_TX_APP_ERROR_KEYWORDS = [
    "ERROR",
    "FATAL",
    "exception",
    "segfault",
    "core dumped",
    "failed",
    "FAIL",
]
