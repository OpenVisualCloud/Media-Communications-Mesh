# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024-2025 Intel Corporation
# Media Communications Mesh
import logging

KEYWORDS = {
    "memif": ' type="memif"',
    "rdma_tx": ' type="rdma" kind="tx"',
    "rdma_rx": ' type="rdma" kind="rx"',
}


def parse_logs(log_line: str) -> str:
    if ' type="' in log_line:
        for connection_type, keyword in KEYWORDS.items():
            logging.debug(f"Checking >>{keyword}<< against >>{log_line}<<")
            if keyword in log_line:
                return connection_type
    return ""
