# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

"""
Common log validation utilities for use by both rxtxapp and ffmpeg validation.
"""
import logging
import os
from typing import List, Tuple, Dict, Any, Optional

from common.log_constants import RX_TX_APP_ERROR_KEYWORDS

logger = logging.getLogger(__name__)


def check_phrases_in_order(
    log_path: str, phrases: List[str]
) -> Tuple[bool, List[str], Dict[str, List[str]]]:
    """
    Check that all required phrases appear in order in the log file.
    Returns (all_found, missing_phrases, context_lines)
    """
    found_indices = []
    missing_phrases = []
    lines_around_missing = {}
    with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
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


def check_for_errors(
    log_path: str, error_keywords: Optional[List[str]] = None
) -> Tuple[bool, List[Dict[str, Any]]]:
    """
    Check the log file for error keywords.

    Args:
        log_path: Path to the log file
        error_keywords: List of keywords indicating errors (default: RX_TX_APP_ERROR_KEYWORDS)

    Returns:
        Tuple of (is_pass, errors_found)
        errors_found is a list of dicts with 'line', 'line_number', and 'keyword' keys
    """
    if error_keywords is None:
        error_keywords = RX_TX_APP_ERROR_KEYWORDS

    errors = []
    if not os.path.exists(log_path):
        logger.error(f"Log file not found: {log_path}")
        return False, [
            {
                "line": "Log file not found",
                "line_number": 0,
                "keyword": "FILE_NOT_FOUND",
            }
        ]

    try:
        with open(log_path, "r", encoding="utf-8", errors="replace") as f:
            for i, line in enumerate(f):
                for keyword in error_keywords:
                    if keyword.lower() in line.lower():
                        # Ignore certain false positives
                        if "ERROR" in keyword and (
                            "NO ERROR" in line.upper() or "NO_ERROR" in line.upper()
                        ):
                            continue
                        errors.append(
                            {
                                "line": line.strip(),
                                "line_number": i + 1,
                                "keyword": keyword,
                            }
                        )
                        break
    except Exception as e:
        logger.error(f"Error reading log file: {e}")
        return False, [
            {
                "line": f"Error reading log file: {e}",
                "line_number": 0,
                "keyword": "FILE_READ_ERROR",
            }
        ]

    return len(errors) == 0, errors


def validate_log_file(
    log_file_path: str,
    required_phrases: List[str],
    direction: str = "",
    error_keywords: Optional[List[str]] = None,
    strict_order: bool = False,
) -> Dict:
    """
    Validate log file for required phrases and return validation information.

    Args:
        log_file_path: Path to the log file
        required_phrases: List of phrases to check for
        direction: Optional string to identify the direction (e.g., 'Rx', 'Tx')
        error_keywords: Optional list of error keywords to check for
        strict_order: Whether to check phrases in strict order (default: False)

    Returns:
        Dictionary containing validation results:
        {
            'is_pass': bool,
            'error_count': int,
            'validation_info': list of validation information strings,
            'missing_phrases': list of missing phrases,
            'context_lines': dict mapping phrases to context lines,
            'errors': list of error dictionaries (if error_keywords provided)
        }
    """
    validation_info = []

    # Phrase validation (either ordered or anywhere)
    if strict_order:
        log_pass, missing, context_lines = check_phrases_in_order(
            log_file_path, required_phrases
        )
    else:
        log_pass, missing, context_lines = check_phrases_anywhere(
            log_file_path, required_phrases
        )
    error_count = len(missing)

    # Error keyword validation (optional)
    error_check_pass = True
    errors = []
    if error_keywords is not None:
        error_check_pass, errors = check_for_errors(log_file_path, error_keywords)
        error_count += len(errors)

    # Overall pass/fail status
    is_pass = log_pass and error_check_pass

    # Build validation info
    dir_prefix = f"{direction} " if direction else ""
    validation_info.append(f"=== {dir_prefix}Log Validation ===")
    validation_info.append(f"Log file: {log_file_path}")
    validation_info.append(f"Validation result: {'PASS' if is_pass else 'FAIL'}")
    validation_info.append(f"Total errors found: {error_count}")

    # Missing phrases info
    if not log_pass:
        validation_info.append(f"Missing or out-of-order phrases analysis:")
        for phrase in missing:
            validation_info.append(f'\n  Expected phrase: "{phrase}"')
            validation_info.append(f"  Context in log file:")
            if phrase in context_lines:
                for line in context_lines[phrase]:
                    validation_info.append(f"    {line}")
            else:
                validation_info.append("    <No context available>")
        if missing:
            logger.warning(
                f"{dir_prefix}process did not pass. First missing phrase: {missing[0]}"
            )

    # Error keywords info
    if errors:
        validation_info.append(f"\nError keywords found:")
        for error in errors:
            validation_info.append(
                f"  Line {error['line_number']} - {error['keyword']}: {error['line']}"
            )

    return {
        "is_pass": is_pass,
        "error_count": error_count,
        "validation_info": validation_info,
        "missing_phrases": missing,
        "context_lines": context_lines,
        "errors": errors,
    }


def save_validation_report(
    report_path: str, validation_info: List[str], overall_status: bool
) -> None:
    """
    Save validation report to a file.

    Args:
        report_path: Path where to save the report
        validation_info: List of validation information strings
        overall_status: Overall pass/fail status
    """
    try:
        os.makedirs(os.path.dirname(report_path), exist_ok=True)
        with open(report_path, "w", encoding="utf-8") as f:
            for line in validation_info:
                f.write(f"{line}\n")
            f.write(f"\n=== Overall Validation Summary ===\n")
            f.write(f"Overall result: {'PASS' if overall_status else 'FAIL'}\n")
        logger.info(f"Validation report saved to {report_path}")
    except Exception as e:
        logger.error(f"Error saving validation report: {e}")


def check_phrases_anywhere(
    log_path: str, phrases: List[str]
) -> Tuple[bool, List[str], Dict[str, List[str]]]:
    """
    Check that all required phrases appear anywhere in the log file, regardless of order.
    Returns (all_found, missing_phrases, context_lines)
    """
    missing_phrases = []
    lines_around_missing = {}

    try:
        with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
            lines = content.split("\n")

        for phrase in phrases:
            # Check if the phrase is contained in any line, not just the entire content
            phrase_found = False
            for line in lines:
                if phrase in line:
                    phrase_found = True
                    break

            if not phrase_found:
                missing_phrases.append(phrase)
                # Find where the phrase should have appeared
                # Just give some context from the end of the log
                context_end = len(lines)
                context_start = max(0, context_end - 10)
                lines_around_missing[phrase] = lines[context_start:context_end]
    except Exception as e:
        logger.error(f"Error reading log file {log_path}: {e}")
        return False, phrases, {"error": [f"Error reading log file: {e}"]}

    return len(missing_phrases) == 0, missing_phrases, lines_around_missing


def output_validator(
    log_file_path: str, error_keywords: Optional[List[str]] = None
) -> Dict[str, Any]:
    """
    Simple validator that checks for error keywords in a log file.

    Args:
        log_file_path: Path to the log file
        error_keywords: List of keywords indicating errors

    Returns:
        Dictionary with validation results:
        {
            'is_pass': bool,
            'errors': list of error dictionaries,
            'phrase_mismatches': list of phrase mismatches (empty in this validator)
        }
    """
    is_pass, errors = check_for_errors(log_file_path, error_keywords)

    return {
        "is_pass": is_pass,
        "errors": errors,
        "phrase_mismatches": [],  # Not used in this simple validator
    }
