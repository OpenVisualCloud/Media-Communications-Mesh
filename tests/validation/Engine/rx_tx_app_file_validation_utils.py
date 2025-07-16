# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import logging

logger = logging.getLogger(__name__)


def validate_file(connection, file_path, cleanup=True):
    """
    Validate a file on a remote host including existence, size, and optional cleanup.
    
    This function performs comprehensive file validation:
    - Checks if the file exists
    - Verifies the file has non-zero size
    - Optionally cleans up the file after validation
    
    Args:
        connection: The connection object to the remote host
        file_path (str): Path to the file to validate
        cleanup (bool): Whether to remove the file after validation (default: True)
        
    Returns:
        tuple: (validation_info, file_validation_passed)
            - validation_info (list): List of validation messages
            - file_validation_passed (bool): Whether all validations passed
    """
    validation_info = []
    file_validation_passed = True

    f = connection.path(file_path)
    if not f.exists():
        error_msg = f"File does not exist: {file_path}"
        logger.warning(error_msg)
        validation_info.append(f"File existence: FAIL - {error_msg}")
        file_validation_passed = False
    else:
        validation_info.append("File existence: PASS")
        
        # Execute a command to get the file size using ls
        result = connection.execute_command(f"ls -l {file_path}", expected_return_codes=None)
        
        if result.return_code != 0:
            error_msg = f"Failed to retrieve file size for {file_path}."
            logger.warning(error_msg)
            validation_info.append(f"File size check: FAIL - {error_msg}")
            file_validation_passed = False
        else:
            # Parse the output to get the file size
            file_info = result.stdout.strip().split()
            file_size = int(file_info[4])  # The size is the 5th element in the split output
            
            validation_info.append(f"File size: {file_size} bytes (checked via ls -l: {file_info})")
            
            if file_size == 0:
                error_msg = f"File size is 0: {file_path}"
                logger.warning(error_msg)
                validation_info.append(f"File size check: FAIL - {error_msg}")
                file_validation_passed = False
            else:
                validation_info.append("File size check: PASS")
    
    if cleanup:
        if cleanup_file(connection, file_path):
            validation_info.append("File cleanup: PASS - File removed successfully")
        else:
            validation_info.append("File cleanup: FAIL - Unable to remove file")

    return validation_info, file_validation_passed


def cleanup_file(connection, file_path):
    """Remove a file from a remote host."""
    try:
        f = connection.path(file_path)
        if f.exists():
            f.unlink()
            logger.debug(f"Removed: {file_path}")
        return True
    except Exception as e:
        logger.warning(f"Failed to remove file {file_path}: {e}")
        return False
