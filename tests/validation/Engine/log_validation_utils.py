# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

"""
Common log validation utilities for use by both rxtxapp and ffmpeg validation.
"""
import logging
from typing import List, Tuple, Dict

logger = logging.getLogger(__name__)

def check_phrases_in_order(log_path: str, phrases: List[str]) -> Tuple[bool, List[str], Dict[str, List[str]]]:
    """
    Check that all required phrases appear in order in the log file.
    Returns (all_found, missing_phrases, context_lines)
    """
    found_indices = []
    missing_phrases = []
    lines_around_missing = {}
    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = [line.strip() for line in f]

    idx = 0
    for phrase_idx, phrase in enumerate(phrases):
        found = False
        phrase_stripped = phrase.strip()
        start_idx = idx  # Remember where we started searching

        while idx < len(lines):
            line_stripped = lines[idx].strip()
            if phrase_stripped in line_stripped:
                found_indices.append(idx)
                found = True
                idx += 1
                break
            idx += 1

        if not found:
            missing_phrases.append(phrase)
            # Store context - lines around where we were searching
            context_start = max(0, start_idx - 3)
            context_end = min(len(lines), start_idx + 7)
            lines_around_missing[phrase] = lines[context_start:context_end]

    return len(missing_phrases) == 0, missing_phrases, lines_around_missing
