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


def append_to_consolidated_validation_report(
    log_dir: str,
    host_name: str,
    component_name: str,
    validation_info: List[str],
    component_status: bool,
) -> None:
    """
    Append component validation results to a consolidated validation report.

    Args:
        log_dir: Base log directory
        host_name: Name of the host
        component_name: Name of the component (e.g., "rx_1", "tx_1")
        validation_info: List of validation information strings
        component_status: Component pass/fail status
    """
    try:
        # Path will be <log_dir>/RxTx/log_validation.log
        report_dir = os.path.join(log_dir, "RxTx")
        os.makedirs(report_dir, exist_ok=True)
        report_path = os.path.join(report_dir, "log_validation.log")

        with open(report_path, "a", encoding="utf-8") as f:
            f.write(f"\n=== {host_name}: {component_name} Validation ===\n")
            for line in validation_info:
                f.write(f"{line}\n")
            f.write(f"Component status: {'PASS' if component_status else 'FAIL'}\n")
            f.write("="*50 + "\n")

        logger.info(
            f"Component validation for {host_name}:{component_name} appended to consolidated report"
        )
    except Exception as e:
        logger.error(f"Error appending to consolidated validation report: {e}")


def write_executor_validation_summary(
    log_dir: str,
    tx_executor=None,
    rx_executors=None
) -> bool:
    """
    Write the consolidated validation summary from tx and rx executors.

    Args:
        log_dir: Base log directory
        tx_executor: Single tx executor or list of tx executors
        rx_executors: Single rx executor or list of rx executors

    Returns:
        bool: Overall pass/fail status
    """
    component_results = {}
    component_hosts = {}

    # Handle tx executors (can be a single executor or a list)
    if tx_executor is not None:
        if isinstance(tx_executor, list):
            for i, executor in enumerate(tx_executor, 1):
                component_name = f"tx_{i}"
                component_results[component_name] = executor.is_pass
                component_hosts[component_name] = executor.host.name
        else:
            # Single tx executor
            component_name = f"tx_{tx_executor.instance_num}" if hasattr(tx_executor, 'instance_num') and tx_executor.instance_num is not None else "tx_1"
            component_results[component_name] = tx_executor.is_pass
            component_hosts[component_name] = tx_executor.host.name

    # Handle rx executors (can be a single executor or a list)
    if rx_executors is not None:
        if isinstance(rx_executors, list):
            for i, executor in enumerate(rx_executors, 1):
                component_name = f"rx_{executor.instance_num}" if hasattr(executor, 'instance_num') and executor.instance_num is not None else f"rx_{i}"
                component_results[component_name] = executor.is_pass
                component_hosts[component_name] = executor.host.name
        else:
            # Single rx executor
            component_name = f"rx_{rx_executors.instance_num}" if hasattr(rx_executors, 'instance_num') and rx_executors.instance_num is not None else "rx_1"
            component_results[component_name] = rx_executors.is_pass
            component_hosts[component_name] = rx_executors.host.name

    # Write consolidated validation report - simpler version that doesn't rely on log_file attributes
    return write_consolidated_validation_summary(log_dir, component_results, component_hosts)


def create_consolidated_validation_report(
    log_dir: str,
    component_results: Dict[str, bool],
    component_hosts: Dict[str, str],
    validation_data: List[Dict]
) -> bool:
    """
    Create a comprehensive consolidated validation report with detailed information
    for each component.

    Args:
        log_dir: Base log directory
        component_results: Dictionary mapping component names to their validation status
        component_hosts: Dictionary mapping component names to their host names
        validation_data: List of dictionaries containing detailed validation data for each component
        
    Returns:
        bool: Overall pass/fail status
    """
    overall_status = all(component_results.values()) if component_results else True

    try:
        report_dir = os.path.join(log_dir, "RxTx")
        os.makedirs(report_dir, exist_ok=True)
        report_path = os.path.join(report_dir, "log_validation.log")
        
        with open(report_path, "w", encoding="utf-8") as f:
            # Summary section
            f.write("=== Consolidated Validation Report ===\n\n")
            
            # Group components by host
            host_components = {}
            for component, status in component_results.items():
                host = component_hosts.get(component, "unknown-host")
                if host not in host_components:
                    host_components[host] = []
                host_components[host].append((component, status))
            
            # Print components grouped by host in the summary
            for host, components in sorted(host_components.items()):
                f.write(f"Host: {host}\n")
                for component, status in sorted(components):
                    f.write(f"  Component: {component} - {'PASS' if status else 'FAIL'}\n")
                
            f.write(f"\n=== Overall Test Result: {'PASS' if overall_status else 'FAIL'} ===\n\n")
            
            # Detailed section
            f.write("=== Individual Component Validation Reports ===\n\n")
            
            # Write detailed validation info for each component
            for component_data in validation_data:
                host = component_data['host']
                component = component_data['component']
                is_pass = component_data['is_pass']
                log_file = component_data.get('log_file', 'Unknown')
                app_validation = component_data.get('app_validation', {})
                
                f.write(f"--- Component: {host}:{component} ---\n")
                
                # Write app validation info
                direction = "Tx" if component.startswith("tx") else "Rx"
                if app_validation:
                    validation_info = app_validation.get('validation_info', [])
                    for line in validation_info:
                        f.write(f"{line}\n")
                else:
                    f.write(f"=== {direction} Log Validation ===\n")
                    f.write(f"Log file: {log_file}\n")
                    f.write(f"Validation result: {'PASS' if is_pass else 'FAIL'}\n")
                    f.write(f"Total errors found: 0\n")
                
                # Additional validation for RX components (file validation)
                if direction == "Rx" and 'file_validation' in component_data and component_data['file_validation']:
                    file_validation = component_data['file_validation']
                    
                    f.write("\n=== Rx Output File Validation ===\n")
                    if 'file_path' in file_validation:
                        f.write(f"Expected output file: {file_validation['file_path']}\n")
                    
                    if 'file_exists' in file_validation:
                        f.write(f"File existence: {'PASS' if file_validation['file_exists'] else 'FAIL'}\n")
                    
                    if 'file_size' in file_validation:
                        f.write(f"File size: {file_validation['file_size']} bytes")
                        if 'file_size_check_output' in file_validation:
                            f.write(f" (checked via ls -l: {file_validation['file_size_check_output']})\n")
                        else:
                            f.write("\n")
                    
                    if 'file_size_check' in file_validation:
                        f.write(f"File size check: {'PASS' if file_validation['file_size_check'] else 'FAIL'}\n")
                
                # Overall component validation summary
                f.write("\n=== Overall Validation Summary ===\n")
                f.write(f"Overall validation: {'PASS' if is_pass else 'FAIL'}\n")
                f.write(f"App log validation: {'PASS' if is_pass else 'FAIL'}\n")
                
                if direction == "Rx" and 'file_validation' in component_data and component_data['file_validation']:
                    file_validation_pass = component_data['file_validation'].get('is_pass', False)
                    f.write(f"File validation: {'PASS' if file_validation_pass else 'FAIL'}\n")
                
                f.write("\n")
            
        logger.info(f"Consolidated validation summary written to {report_path} with result: {'PASS' if overall_status else 'FAIL'}")
        return overall_status
    except Exception as e:
        logger.error(f"Error writing consolidated validation report: {e}")
        return False


def write_consolidated_validation_summary(
    log_dir: str,
    component_results: Optional[Dict[str, bool]] = None,
    component_hosts: Optional[Dict[str, str]] = None
) -> bool:
    """
    Write the overall validation summary to the consolidated report.

    Args:
        log_dir: Base log directory
        component_results: Dictionary mapping component names to their validation status
        component_hosts: Dictionary mapping component names to their host names

    Returns:
        bool: Overall pass/fail status
    """
    if component_results is None:
        component_results = {}

    if component_hosts is None:
        component_hosts = {}

    overall_status = all(component_results.values()) if component_results else True

    try:
        report_dir = os.path.join(log_dir, "RxTx")
        os.makedirs(report_dir, exist_ok=True)
        report_path = os.path.join(report_dir, "log_validation.log")
        
        with open(report_path, "a", encoding="utf-8") as f:
            f.write("\n" + "="*50 + "\n")
            f.write("=== OVERALL TEST VALIDATION SUMMARY ===\n")
            f.write(f"Overall status: {'PASS' if overall_status else 'FAIL'}\n")
            
            if component_results:
                f.write("\nComponent Status by Host:\n")
                
                # Group components by host
                host_components = {}
                for component, status in component_results.items():
                    host = component_hosts.get(component, "unknown-host")
                    if host not in host_components:
                        host_components[host] = []
                    host_components[host].append((component, status))
                
                # Print components grouped by host
                for host, components in sorted(host_components.items()):
                    f.write(f"\n  {host}:\n")
                    for component, status in sorted(components):
                        f.write(f"    - {component}: {'PASS' if status else 'FAIL'}\n")
                    
            f.write("="*50 + "\n\n")
        
        logger.info(f"Consolidated validation summary written: {'PASS' if overall_status else 'FAIL'}")
        return overall_status
    except Exception as e:
        logger.error(f"Error writing consolidated validation summary: {e}")
        return False


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
