# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024-2025 Intel Corporation
# Media Communications Mesh
import logging

KEYWORDS = {
    "memif": 'INFO - memif connected!',
    "rdma": ' type="rdma" kind="',
}


def parse_logs(log_line: str, expected_test_type:str) -> str:
    if KEYWORDS.get(expected_test_type, 'INFO - memif connected!') in log_line:
        return connection_type
    return ""
