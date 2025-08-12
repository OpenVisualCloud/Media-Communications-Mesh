# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024-2025 Intel Corporation
# Media Communications Mesh

import argparse
import hashlib
import logging
import multiprocessing
import time
import sys
from pathlib import Path


DEFAULT_CHUNK_SIZE = 1024 * 1024  # 1MB chunks for processing
SEGMENT_DURATION_GRACE_PERIOD = (
    1  # Grace period in seconds to allow for late-arriving files
)


def calculate_chunk_hashes(file_url: str, chunk_size: int = DEFAULT_CHUNK_SIZE) -> list:
    """Calculate MD5 hash for each chunk of the file."""
    chunk_sums = []
    with open(file_url, "rb") as f:
        chunk_index = 0
        while chunk := f.read(chunk_size):
            if len(chunk) != chunk_size:
                logging.debug(
                    f"CHUNK SIZE MISMATCH at index {chunk_index}: {len(chunk)} != {chunk_size}"
                )
            chunk_sum = hashlib.md5(chunk).hexdigest()
            chunk_sums.append(chunk_sum)
            chunk_index += 1
    return chunk_sums


def calculate_file_hash(file_url: str) -> str:
    """Calculate MD5 hash of the entire file."""
    hash_md5 = hashlib.md5()
    with open(file_url, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()


class BlobIntegritor:
    def __init__(
        self,
        logger: logging.Logger,
        src_url: str,
        out_name: str,
        chunk_size: int = DEFAULT_CHUNK_SIZE,
        out_path: str = "/mnt/ramdisk",
        delete_file: bool = True,
    ):
        self.logger = logger
        self.logger.info(
            f"Verify blob integrity for src {src_url} out file names: {out_name}\nchunk_size: {chunk_size}"
        )
        self.chunk_size = chunk_size
        self.src_chunk_sums = calculate_chunk_hashes(src_url, self.chunk_size)
        self.src_file_hash = calculate_file_hash(src_url)
        self.src_url = src_url
        self.out_name = out_name
        self.out_path = out_path
        self.out_frame_no = 0
        self.bad_chunks_total = 0
        self.delete_file = delete_file

    def check_integrity_file(self, out_url) -> bool:
        """Check integrity of a single output file against source."""
        out_chunk_sums = calculate_chunk_hashes(out_url, self.chunk_size)
        out_file_hash = calculate_file_hash(out_url)

        # First check if file hashes match (quick check)
        if out_file_hash == self.src_file_hash:
            self.logger.info(f"File hash matches for {out_url}")
            return True

        # If file hashes don't match, check chunk by chunk for detailed analysis
        bad_chunks = 0
        min_chunks = min(len(out_chunk_sums), len(self.src_chunk_sums))

        for ids in range(min_chunks):
            if out_chunk_sums[ids] != self.src_chunk_sums[ids]:
                self.logger.error(f"Received bad chunk number {ids} in {out_url}.")
                bad_chunks += 1

        # Check for size mismatches
        if len(out_chunk_sums) != len(self.src_chunk_sums):
            self.logger.error(
                f"Chunk count mismatch: source has {len(self.src_chunk_sums)} chunks, "
                f"output has {len(out_chunk_sums)} chunks"
            )
            bad_chunks += abs(len(out_chunk_sums) - len(self.src_chunk_sums))

        if bad_chunks:
            self.bad_chunks_total += bad_chunks
            self.logger.error(
                f"Received {bad_chunks} bad chunks out of {len(out_chunk_sums)} total chunks."
            )
            return False

        self.logger.info(f"All {len(out_chunk_sums)} chunks in {out_url} are correct.")
        return True

    def _worker(self, wkr_id, request_queue, result_queue, shared):
        """Worker process for checking file integrity."""
        while True:
            out_url = request_queue.get()
            self.logger.info(f"worker {wkr_id} Checking file: {out_url}")
            if out_url:
                if out_url == "Done":
                    break
                result = self.check_integrity_file(out_url)
                if not result and shared.get("error_file", None) is None:
                    shared["error_file"] = out_url
                    self.logger.debug(f"Saved error file {out_url} for later analysis.")
                    continue
                if self.delete_file:
                    out_url.unlink()
        result_queue.put(self.bad_chunks_total)


class BlobStreamIntegritor(BlobIntegritor):
    """Class to check integrity of blob data saved into files divided by segments."""

    def __init__(
        self,
        logger: logging.Logger,
        src_url: str,
        out_name: str,
        chunk_size: int = DEFAULT_CHUNK_SIZE,
        out_path: str = "/mnt/ramdisk",
        segment_duration: int = 3,
        workers_count=5,
        delete_file: bool = True,
    ):
        super().__init__(logger, src_url, out_name, chunk_size, out_path, delete_file)
        self.logger.info(
            f"Output path {out_path}, segment duration: {segment_duration}"
        )
        self.segment_duration = segment_duration
        self.workers_count = workers_count

    def get_out_file(self, request_queue):
        """Monitor output path for new files and add them to processing queue."""
        self.logger.info(
            f"Waiting for output files from {self.out_path} with prefix {self.out_name}"
        )
        start = 0
        waiting_for_files = True
        list_processed = []
        out_files = []

        # Add a grace period to segment duration to allow for late-arriving files
        while (
            self.segment_duration + SEGMENT_DURATION_GRACE_PERIOD > start
        ) or waiting_for_files:
            gb = list(Path(self.out_path).glob(f"{self.out_name}*"))
            out_files = list(filter(lambda x: x not in list_processed, gb))
            self.logger.debug(f"Received files: {out_files}")
            if len(out_files) > 1:
                self.logger.debug(f"Putting file into queue {out_files[0]}")
                request_queue.put(out_files[0])
                list_processed.append(out_files[0])
                waiting_for_files = False
                start = 0
            start += 0.5
            time.sleep(0.5)
        else:
            try:
                request_queue.put(out_files[0])
            except IndexError:
                self.logger.info(f"No more output files found in {self.out_path}")

    def worker(self, wkr_id, request_queue, result_queue, shared):
        """Worker process that handles file integrity checking."""
        self._worker(wkr_id, request_queue, result_queue, shared)

    def start_workers(self, result_queue, request_queue, shared):
        """Create and start worker processes."""
        processes = []
        for number in range(self.workers_count):
            p = multiprocessing.Process(
                target=self.worker, args=(number, request_queue, result_queue, shared)
            )
            p.start()
            processes.append(p)
        return processes

    def check_blob_integrity(self) -> bool:
        """Check integrity of blob data across multiple segmented files."""
        mgr = multiprocessing.Manager()
        shared_data = mgr.dict()
        result_queue = multiprocessing.Queue()
        request_queue = multiprocessing.Queue()
        workers = self.start_workers(result_queue, request_queue, shared_data)
        output_worker = multiprocessing.Process(
            target=self.get_out_file, args=(request_queue,)
        )
        output_worker.start()
        results = []
        while output_worker.is_alive():
            time.sleep(1)
        output_worker.join()
        for wk in workers:
            request_queue.put("Done")
        for wk in workers:
            results.append(result_queue.get())
            wk.join()
        logging.info(f"Results of bad chunks: {results}")
        return True if shared_data.get("error_file", None) is None else False


class BlobFileIntegritor(BlobIntegritor):
    """Class to check integrity of a single blob file."""

    def __init__(
        self,
        logger: logging.Logger,
        src_url: str,
        out_name: str,
        chunk_size: int = DEFAULT_CHUNK_SIZE,
        out_path: str = "/mnt/ramdisk",
        delete_file: bool = True,
    ):
        super().__init__(logger, src_url, out_name, chunk_size, out_path, delete_file)
        self.logger = logging.getLogger(__name__)

    def get_out_file(self):
        """Find the output file to check."""
        try:
            gb = list(Path(self.out_path).glob(f"{self.out_name}*"))
            return gb[0]
        except IndexError:
            self.logger.error(f"File {self.out_name} not found!")
            raise FileNotFoundError(
                f"File {self.out_name} not found in {self.out_path}"
            )

    def check_blob_integrity(self) -> bool:
        """Check integrity of a single blob file."""
        output_file = self.get_out_file()
        result = self.check_integrity_file(output_file)
        if self.delete_file and result:
            output_file.unlink()
        return result


def main():
    # Set up logging
    logging.basicConfig(
        format="%(levelname)s:%(message)s",
        level=logging.DEBUG,
        handlers=[logging.FileHandler("blob_integrity.log"), logging.StreamHandler()],
    )
    logger = logging.getLogger(__name__)

    # Define the argument parser
    parser = argparse.ArgumentParser(
        description="""Check Integrity of blob files with source one. Supports binary data integrity checking.

Two modes available: stream and file.
Stream mode is for blob data saved into files segmented by time, while file mode is for single blob files.
Run script with mode type and --help for more information.

Example of command: 
python blob_integrity.py stream src.bin out_name --output_path /mnt/ramdisk --segment_duration 3 --workers 5 --chunk_size 1048576

The tool compares files chunk by chunk using MD5 hashes and also performs a full file hash comparison.""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    subparsers = parser.add_subparsers(
        dest="mode", help="Mode of operation: stream or file"
    )

    # Common arguments function to avoid repetition
    def add_common_arguments(parser):
        parser.add_argument("src", type=str, help="Path to the source blob file")
        parser.add_argument("out", type=str, help="Name/prefix of the output file")
        parser.add_argument(
            "--chunk_size",
            type=int,
            default=DEFAULT_CHUNK_SIZE,
            help=f"Chunk size for processing in bytes (default: {DEFAULT_CHUNK_SIZE})",
        )
        parser.add_argument(
            "--output_path",
            type=str,
            default="/mnt/ramdisk",
            help="Output path (default: /mnt/ramdisk)",
        )
        parser.add_argument(
            "--delete_file",
            action="store_true",
            default=True,
            help="Delete output files after processing (default: True)",
        )
        parser.add_argument(
            "--no_delete_file",
            action="store_false",
            dest="delete_file",
            help="Do NOT delete output files after processing",
        )

    # Stream mode parser
    stream_help = """Check integrity for blob stream (blob data saved into files segmented by time)

It assumes that there is X digit segment number in the file name like `out_name_001.bin` or `out_name_02.bin`.
This can be achieved by splitting large blob files into smaller segments.

Example: split -b 1M input.bin out_name_"""
    stream_parser = subparsers.add_parser(
        "stream",
        help="Check integrity for blob stream (segmented files)",
        description=stream_help,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    add_common_arguments(stream_parser)
    stream_parser.add_argument(
        "--segment_duration",
        type=int,
        default=3,
        help="Segment duration in seconds (default: 3)",
    )
    stream_parser.add_argument(
        "--workers",
        type=int,
        default=5,
        help="Number of worker processes to use (default: 5)",
    )

    # File mode parser
    file_help = """Check integrity for single blob file.

This mode compares a single output blob file against a source reference file.
It performs chunk-by-chunk integrity checking using MD5 checksums."""
    file_parser = subparsers.add_parser(
        "file",
        help="Check integrity for single blob file",
        description=file_help,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    add_common_arguments(file_parser)

    # Parse the arguments
    args = parser.parse_args()

    # Execute based on mode
    if args.mode == "stream":
        integrator = BlobStreamIntegritor(
            logger,
            args.src,
            args.out,
            args.chunk_size,
            args.output_path,
            args.segment_duration,
            args.workers,
            args.delete_file,
        )
    elif args.mode == "file":
        integrator = BlobFileIntegritor(
            logger,
            args.src,
            args.out,
            args.chunk_size,
            args.output_path,
            args.delete_file,
        )
    else:
        parser.print_help()
        return

    result = integrator.check_blob_integrity()
    if result:
        logging.info("Blob integrity check passed")
    else:
        logging.error("Blob integrity check failed")
        sys.exit(1)


if __name__ == "__main__":
    main()
