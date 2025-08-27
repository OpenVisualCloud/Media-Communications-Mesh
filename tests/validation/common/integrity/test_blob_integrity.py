#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024-2025 Intel Corporation
# Media Communications Mesh

"""
Simple test script to demonstrate blob integrity checking functionality.
This script generates test blob files and verifies the integrity checker works correctly.
"""

import logging
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

# Import the blob integrity classes
from blob_integrity import BlobFileIntegrator, BlobStreamIntegrator


def generate_test_blob(file_path: str, size_mb: int = 10):
    """Generate a test blob file using dd command."""
    size_bytes = size_mb * 1024 * 1024
    cmd = [
        "dd",
        "if=/dev/urandom",  # Using urandom instead of random for better performance
        f"of={file_path}",
        "bs=1M",
        f"count={size_mb}",
    ]

    print(f"Generating test blob: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        raise RuntimeError(f"Failed to generate test blob: {result.stderr}")

    print(f"Generated {size_mb}MB test blob at {file_path}")
    return file_path


def test_file_integrity():
    """Test single file blob integrity checking."""
    print("\n=== Testing File Blob Integrity ===")

    with tempfile.TemporaryDirectory() as temp_dir:
        # Generate source blob
        src_path = os.path.join(temp_dir, "source.bin")
        generate_test_blob(src_path, 5)  # 5MB test file

        # Copy source to create "output" file
        out_path = os.path.join(temp_dir, "output.bin")
        shutil.copy2(src_path, out_path)

        # Setup logger
        logger = logging.getLogger("test_file")

        # Test integrity check
        integrator = BlobFileIntegrator(
            logger=logger,
            src_url=src_path,
            out_name="output.bin",
            chunk_size=1024 * 256,  # 256KB chunks
            out_path=temp_dir,
            delete_file=False,  # Don't delete for testing
        )

        result = integrator.check_blob_integrity()
        print(f"File integrity check result: {'PASSED' if result else 'FAILED'}")
        return result


def test_corrupted_file_integrity():
    """Test blob integrity checking with a corrupted file."""
    print("\n=== Testing Corrupted File Detection ===")

    with tempfile.TemporaryDirectory() as temp_dir:
        # Generate source blob
        src_path = os.path.join(temp_dir, "source.bin")
        generate_test_blob(src_path, 5)  # 5MB test file

        # Create corrupted copy
        out_path = os.path.join(temp_dir, "corrupted.bin")
        shutil.copy2(src_path, out_path)

        # Corrupt the file by modifying some bytes in the middle
        with open(out_path, "r+b") as f:
            f.seek(1024 * 1024)  # Seek to 1MB position
            f.write(b"\xFF" * 1024)  # Write 1KB of 0xFF bytes

        # Setup logger
        logger = logging.getLogger("test_corrupted")

        # Test integrity check
        integrator = BlobFileIntegrator(
            logger=logger,
            src_url=src_path,
            out_name="corrupted.bin",
            chunk_size=1024 * 256,  # 256KB chunks
            out_path=temp_dir,
            delete_file=False,  # Don't delete for testing
        )

        result = integrator.check_blob_integrity()
        print(
            f"Corrupted file integrity check result: {'PASSED (UNEXPECTED!)' if result else 'FAILED (EXPECTED)'}"
        )
        return not result  # We expect this to fail, so return True if it failed


def main():
    """Run blob integrity tests."""
    # Setup logging
    logging.basicConfig(
        format="%(levelname)s:%(name)s:%(message)s",
        level=logging.INFO,
        handlers=[logging.StreamHandler()],
    )

    print("Blob Integrity Checker Test Suite")
    print("=" * 40)

    try:
        # Test normal file integrity
        test1_passed = test_file_integrity()

        # Test corrupted file detection
        test2_passed = test_corrupted_file_integrity()

        print("\n=== Test Results ===")
        print(f"File integrity test: {'PASSED' if test1_passed else 'FAILED'}")
        print(f"Corruption detection test: {'PASSED' if test2_passed else 'FAILED'}")

        if test1_passed and test2_passed:
            print("\nAll tests PASSED! ✓")
            return 0
        else:
            print("\nSome tests FAILED! ✗")
            return 1

    except Exception as e:
        print(f"\nTest suite failed with error: {e}")
        return 1


if __name__ == "__main__":
    exit(main())
