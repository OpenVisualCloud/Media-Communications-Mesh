# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024-2025 Intel Corporation
# Media Communications Mesh

import hashlib
import logging
import re
import time
from math import floor
from pathlib import Path


def calculate_chunk_hashes(file_url: str, chunk_size: int) -> list:
    chunk_sums = []
    with open(file_url, "rb") as f:
        chunk_index = 0  # Initialize a counter for the chunk index
        while chunk := f.read(chunk_size):
            if len(chunk) != chunk_size:
                logging.debug(f"CHUNK SIZE MISMATCH at index {chunk_index}: {len(chunk)} != {chunk_size}")
            chunk_sum = hashlib.md5(chunk).hexdigest()
            chunk_sums.append(chunk_sum)
            chunk_index += 1  # Increment the chunk index
    return chunk_sums


def check_chunk_integrity(src_chunk_sums, out_chunk_sums, expected_frame_percentage: int = 80, looped: bool = False) -> bool:
    if looped:
        logging.debug(f"Source chunks: {len(src_chunk_sums)}, Output chunks: {len(out_chunk_sums)}")

        repeat_count = len(out_chunk_sums) // len(src_chunk_sums)
        remainder = len(out_chunk_sums) % len(src_chunk_sums)

        if len(out_chunk_sums) < len(src_chunk_sums):
            received_percentage = len(out_chunk_sums) / len(src_chunk_sums) * 100.00
            if received_percentage < expected_frame_percentage:
                logging.error(
                    f"Received only {received_percentage:.2f}% of the frames! Expected: {expected_frame_percentage:.2f}%")
                return False
            else:
                logging.debug(f"Received {received_percentage:.2f}% of the frames")

        for i in range(repeat_count):
            for j in range(len(src_chunk_sums)):
                if out_chunk_sums[i * len(src_chunk_sums) + j] != src_chunk_sums[j]:
                    logging.error(f"Mismatch at repeat {i}, index {j}")
                    return False

        for j in range(remainder):
            if out_chunk_sums[repeat_count * len(src_chunk_sums) + j] != src_chunk_sums[j]:
                logging.error(f"Mismatch at remainder index {j}")
                return False

        logging.debug("Integrity check passed with repeated data.")
        return True
    else:
        # ensure enough frames were sent
        if len(out_chunk_sums) > len(src_chunk_sums):
            logging.error(f"Received {len(out_chunk_sums) - len(src_chunk_sums)} more frames than sent!")
            return False

        received_percentage = len(out_chunk_sums) / len(src_chunk_sums) * 100.00
        if received_percentage < expected_frame_percentage:
            logging.error(
                f"Received only {received_percentage:.2f}% of the frames! Expected: {expected_frame_percentage:.2f}%")
            return False
        else:  # log and continue
            logging.debug(f"Received {received_percentage:.2f}% of the frames")

        # loop helpers
        src_index = 0
        out_index = 0
        first_shift = True

        # until the end of any chunk list is met
        while out_index < len(out_chunk_sums) and src_index < len(src_chunk_sums):
            # if any output frame not in input frames, fail
            if out_chunk_sums[out_index] not in src_chunk_sums:
                return False
            # if the same, move on and disable first_shift
            if out_chunk_sums[out_index] == src_chunk_sums[src_index]:
                out_index += 1
                src_index += 1
                first_shift = False
                continue
            # if first time shift, shift source and continue
            if first_shift:
                src_index += 1
                continue
            # not a first shift, corrupted in the middle
            return False
        # only return true if no more frames left in out frames list, else fail
        return out_index == len(out_chunk_sums)

def check_integrity_big_file(src_chunk_sums, out_chunk_sums, allowed_bad: int = 10) -> bool:
    logging.debug("Starting integrity check for big file.")
    logging.debug(f"Source chunk sums: {len(src_chunk_sums)} chunks")
    logging.debug(f"Output chunk sums: {len(out_chunk_sums)} chunks")
    logging.debug(f"Allowed bad frames: {allowed_bad}")

    # Find the first correct frame within the allowed_bad range
    for frame_start in range(min(allowed_bad, len(out_chunk_sums))):
        if out_chunk_sums[frame_start] in src_chunk_sums:
            shift = src_chunk_sums.index(out_chunk_sums[frame_start])
            logging.debug(f"Found first correct frame at position {frame_start} with shift {shift}.")
            break
    else:
        logging.error(f"Did not find any correct frame in the first {allowed_bad} frames of the output.")
        return False

    # Shift the source chunk list
    src_chunk_sums = src_chunk_sums[shift:] + src_chunk_sums[:shift]
    logging.debug(f"Shifting source chunks by {shift} positions.")

    # Check the integrity of the remaining frames
    bad_frames = 0
    for ids, chunk_sum in enumerate(out_chunk_sums):
        if chunk_sum != src_chunk_sums[ids % len(src_chunk_sums)]:
            bad_frames += 1
            logging.debug(f"Bad frame detected at position {ids}.")

    if bad_frames > allowed_bad:
        logging.error(f"Received {bad_frames} bad frames out of {len(out_chunk_sums)} captured.")
        return False

    logging.debug("Integrity check passed with no bad frames.")
    return True

def calculate_yuv_frame_size(width: int, height: int, file_format: str) -> int:
    match file_format:
        case "YUV422RFC4175PG2BE10" | "yuv422p10rfc4175":
            pixel_size = 2.5
        case "YUV422PLANAR10LE" | "yuv422p10le":
            pixel_size = 4
        case _:
            logging.error(f"Size of {file_format} pixel is not known")

    return int(width * height * pixel_size)


def check_integrity(src_url: str, out_name: str, frame_size: int, expected_frame_percentage: int = 80) -> bool:
    src_chunk_sums = calculate_chunk_hashes(src_url, frame_size)
    out_chunk_sums = calculate_chunk_hashes(out_name, frame_size)
    return check_chunk_integrity(src_chunk_sums, out_chunk_sums, expected_frame_percentage)


# TODO: Get rid of the need of backwards compatibility
def check_st20p_integrity(src_url: str, out_name: str, frame_size: int, expected_frame_percentage: int = 80) -> bool:
    return check_integrity(src_url, out_name, frame_size, expected_frame_percentage)


# TODO: Get rid of the need of backwards compatibility
def check_st30p_integrity(src_url: str, out_name: str, frame_size: int, expected_frame_percentage: int = 80) -> bool:
    return check_integrity(src_url, out_name, frame_size, expected_frame_percentage)


def check_st30p_integrity_big(src_url: str, out_url: str, fmt: str, ptime: str, sampling: str, channel: str) -> bool:
    frame_size = calculate_st30p_framebuff_size(fmt, ptime, sampling, channel)
    src_chunk_sums = calculate_chunk_hashes(src_url, frame_size)
    out_chunk_sums = calculate_chunk_hashes(out_url, frame_size)
    return check_integrity_big_file(src_chunk_sums, out_chunk_sums)

def calculate_st30p_framebuff_size(
        format: str, ptime: str, sampling: str, channel: str
) -> int:
    match format:
        case "PCM8":
            sample_size = 1
        case "PCM16":
            sample_size = 2
        case "PCM24":
            sample_size = 3

    match sampling:
        case "41.1kHz":
            match ptime:
                case "1.09":
                    sample_num = 45
        case "48kHz":
            match ptime:
                case "1":
                    sample_num = 48
                case "0.12":
                    sample_num = 6
                case "0.25":
                    sample_num = 12
                case "0.33":
                    sample_num = 16
                case "4":
                    sample_num = 192
        case "96kHz":
            match ptime:
                case "1":
                    sample_num = 96
                case "0.12":
                    sample_num = 12
                case "0.25":
                    sample_num = 24
                case "0.33":
                    sample_num = 32
                case "4":
                    sample_num = 384

    match channel:
        case "M":
            channel_num = 1
        case "DM" | "ST" | "LtRt" | "AES3":
            channel_num = 2
        case "51":
            channel_num = 6
        case "71":
            channel_num = 8
        case "222":
            channel_num = 24
        case "SGRP":
            channel_num = 4
        case _:
            match = re.match(r"^U(\d{2})$", channel)

            if match:
                channel_num = int(match.group(1))

    packet_size = sample_size * sample_num * channel_num

    match ptime:
        case "1":
            packet_time = 1_000_000 * 1
        case "0.12":
            packet_time = 1_000_000 * 0.125
        case "0.25":
            packet_time = 1_000_000 * 0.25
        case "0.33":
            packet_time = 1_000_000 * 1 / 3
        case "1.09":
            packet_time = 1_000_000 * 1.09
        case "4":
            packet_time = 1_000_000 * 4

    desired_frame_time = 10_000_000

    packet_per_frame = 1

    if desired_frame_time > packet_time:
        packet_per_frame = floor(desired_frame_time / packet_time)

    return packet_per_frame * packet_size


class StreamIntegritor:

    def __init__(self, src_url: str, out_name: str, resolution: str = None, file_format: str = None, out_path: str = "/mnt/ramdisk",
                 segment_duration: int = 3):
        if resolution and file_format:
            logging.info(
                f"Verify integrity for src {src_url} out file names: {out_name}\nresolution: {resolution}, file_format: {file_format}")
            logging.info(f"Output path {out_path}, segment duration: {segment_duration}")
            width, height = map(int, resolution.split("x"))
            self.frame_size = calculate_yuv_frame_size(width, height, file_format)
            self.src_chunk_sums = calculate_chunk_hashes(src_url, self.frame_size)
        else:
            logging.info(
                f"Verify integrity for audio stream src {src_url} out file names: {out_name}")
            self.frame_size = None
            self.src_chunk_sums = None
        logging.info(f"Output path {out_path}, segment duration: {segment_duration}")
        self.src_url = src_url
        self.out_name = out_name
        self.out_path = out_path
        self.shift = 0
        self.segment_duration = segment_duration
        self.out_frame_no = 0
        self.bad_frames_total = 0

    def get_first_frame(self, out_url, allowed_bad: int = 10):
        logging.info("Calculate md5 of frames from first file and find first frame then shift src chunk list.")
        out_chunk_sums = calculate_chunk_hashes(out_url, self.frame_size)
        frame_start = 0
        # find first correct frame
        while allowed_bad > frame_start:
            if out_chunk_sums[frame_start] in self.src_chunk_sums:
                self.shift = self.src_chunk_sums.index(out_chunk_sums[frame_start])
                break
            frame_start += 1
        else:
            logging.error(f"Did not found correct frame in first {allowed_bad} frames of the captured video")
            return False

        for _ in range(self.shift):
            self.src_chunk_sums.append(self.src_chunk_sums.pop(0))
        logging.info(f"Src chunk list shifted by {self.shift}")
        logging.debug("List of src chunks: ")
        logging.debug(self.src_chunk_sums)
        return out_chunk_sums

    def get_out_file(self):
        start = 0
        while self.segment_duration * 2 > start:
            out_files = list(Path(self.out_path).glob(f"{self.out_name}*"))
            if len(out_files) > 0:
                return out_files[0]
            start += 1
            time.sleep(1)
        try:
            return out_files[0]
        except IndexError:
            logging.info(f"No more output files found in {self.out_path}")
            return None

    def check_st20p_integrity(self) -> bool:
        out_url = self.get_out_file()
        logging.info(f"Checking first file: {out_url}")
        out_chunk_sums = self.get_first_frame(out_url)
        self.check_integrity_file(out_chunk_sums, out_url)
        out_url.unlink()
        while True:
            out_url = self.get_out_file()
            if out_url:
                logging.info(f"Checking file: {out_url}")
                out_chunk_sums = calculate_chunk_hashes(out_url, self.frame_size)
                if self.check_integrity_file(out_chunk_sums, out_url):
                    logging.info(f"{out_url} PASS with no error, removing file.")
                out_url.unlink()
            else:
                logging.info(f"Totally found {self.bad_frames_total} bad frames.")
                break

    def check_st30p_integrity(self, fmt, ptime, sampling, channel, looped=False) -> dict:
        out_url = self.get_out_file()
        if out_url is None:
            logging.error("No output files found to check integrity.")
            return {"status": "failed", "reason": "No output files found"}

        logging.info(f"Checking first audio file: {out_url}")
        # Calculate frame size for audio streams
        self.frame_size = calculate_st30p_framebuff_size(fmt, ptime, sampling, channel)
        self.src_chunk_sums = calculate_chunk_hashes(self.src_url, self.frame_size)

        # Use get_first_frame to align and get the initial chunk sums
        out_chunk_sums = self.get_first_frame(out_url)
        if out_chunk_sums is False:
            return {"status": "failed", "reason": "Failed to find correct initial frame"}

        # Use check_integrity_file to perform the integrity check
        result = self.check_integrity_file(out_chunk_sums, out_url)
        out_url.unlink()

        # Prepare detailed result information
        total_frames = len(out_chunk_sums)
        bad_frames = self.bad_frames_total
        passed = result and bad_frames == 0

        # Log detailed information
        logging.info(f"Total frames checked: {total_frames}")
        logging.info(f"Bad frames found: {bad_frames}")
        logging.info(f"Integrity check {'passed' if passed else 'failed'}.")

        # Return detailed result information
        return {
            "status": "passed" if passed else "failed",
            "total_frames": total_frames,
            "bad_frames": bad_frames,
            "looped": looped
        }

    def check_integrity_file(self, out_chunk_sums, out_url) -> bool:
        bad_frames = 0
        for ids, chunk_sum in enumerate(out_chunk_sums):
            if chunk_sum != self.src_chunk_sums[ids % len(self.src_chunk_sums)]:
                logging.error(f"Received bad frame number {ids} in {out_url}.")
                if chunk_sum in self.src_chunk_sums:
                    logging.error(
                        f"Chunk: {chunk_sum} received in position: {ids}, expected in {self.src_chunk_sums.index(chunk_sum)}")
                else:
                    logging.error(f"Chunk: {chunk_sum} with ID: {ids} not found in src chunks checksums!")
                bad_frames += 1
        if bad_frames:
            self.bad_frames_total += bad_frames
            logging.error(f"Received {bad_frames} bad frames out of {len(out_chunk_sums)} captured.")
            return False
        return True


if __name__ == "__main__":
    help = """
        Check Integrity of big files with source one. Supports check_st20p_integrity_big and check_st30p_integrity_big methods.
        
        Usage examples:

        For ST20P (video stream):
        >$ python integrity.py stream path_to_src_file out_filename resolution file_format
        - path_to_src_file: Path to the source video file.
        - out_filename: Prefix of the output file in segment using ffmpeg.
        - resolution: Resolution of the video (e.g., 1920x1080).
        - file_format: Format of the video file (e.g., yuv422p10le).

        Example:
        ffmpeg -f mcm -conn_type st2110 -ip_addr 239.168.85.20 -port 20000 -frame_rate 60 -video_size 1920x1080
            -pixel_format yuv422p10le -i - -f rawvideo -f segment -segment_time 3 /mnt/ramdisk/1920x1080_rx_out.yuv
        In this case, the output_filename is: 1920x1080_rx_out
        >$ python integrity.py stream /mnt/media/HDR_1920x1080.yuv 1920x1080_rx_out 1920x1080 yuv422p10le

        Additional parameters:
        - output_path: Default is /mnt/ramdisk
        - segment_duration: Default is 3 seconds

        For ST30P (audio stream):
        >$ python integrity.py file path_to_src_file path_to_out_file format ptime sampling channel
        - path_to_src_file: Path to the source audio file.
        - path_to_out_file: Path to the output audio file.
        - format: Audio format (e.g., PCM8, PCM16, PCM24).
        - ptime: Packet time (e.g., 1, 0.12, 0.25).
        - sampling: Sampling rate (e.g., 41.1kHz, 48kHz, 96kHz).
        - channel: Channel configuration (e.g., M, DM, ST).

        Example:
        >$ python integrity.py file /mnt/media/audio_src.pcm /mnt/ramdisk/audio_out.pcm PCM8 1.09 41.1kHz M

        Note: For looped data, add 'true' or 'false' as the last argument to specify if the data is looped.
    """
    import sys
    import logging

    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.DEBUG,
                        handlers=[logging.FileHandler("integrity.log"), logging.StreamHandler()])

    try:
        if len(sys.argv) < 2:
            print(help)
            exit()

        mode = sys.argv[1].lower()

        if mode == "stream":
            if len(sys.argv) == 6:
                _, _, src, out, res, fmt = sys.argv
                out_path = "/mnt/ramdisk"
                segment_duration = 3
                StreamIntegritor(src, out, res, fmt, out_path, segment_duration).check_st20p_integrity()
            elif len(sys.argv) == 7:
                _, _, src, out, res, fmt, out_path = sys.argv
                segment_duration = 3
                StreamIntegritor(src, out, res, fmt, out_path, segment_duration).check_st20p_integrity()
            elif len(sys.argv) == 8:
                _, _, src, out, fmt, ptime, sampling, channel = sys.argv
                out_path = "/mnt/ramdisk"
                segment_duration = 3
                StreamIntegritor(src, out, out_path=out_path, segment_duration=segment_duration).check_st30p_integrity(fmt, ptime, sampling, channel)
            elif len(sys.argv) == 9:
                _, _, src, out, fmt, ptime, sampling, channel, out_path = sys.argv
                segment_duration = 3
                StreamIntegritor(src, out, out_path=out_path, segment_duration=segment_duration).check_st30p_integrity(fmt, ptime, sampling, channel)
            elif len(sys.argv) == 10:
                _, _, src, out, fmt, ptime, sampling, channel, out_path, looped = sys.argv
                segment_duration = 3
                looped = looped.lower() == 'true'
                StreamIntegritor(src, out, out_path=out_path, segment_duration=segment_duration).check_st30p_integrity(fmt, ptime, sampling, channel, looped)
            else:
                print(help)
        elif mode == "file":
            if len(sys.argv) == 8:
                _, _, src, out, fmt, ptime, sampling, channel = sys.argv
                print(check_st30p_integrity_big(src, out, fmt, ptime, sampling, channel))
            else:
                print(help)
        else:
            print(help)
    except IndexError:
        print(help)
        exit()