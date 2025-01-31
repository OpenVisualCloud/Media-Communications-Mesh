# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024-2025 Intel Corporation
# Media Communications Mesh

import hashlib
import logging
import re
from math import floor

from Engine.execute import log_fail


def calculate_chunk_hashes(file_url: str, chunk_size: int) -> list:
    chunk_sums = []
    with open(file_url, "rb") as f:
        while chunk := f.read(chunk_size):
            if len(chunk) != chunk_size:
                logging.debug(f"CHUNK SIZE MISMATCH {len(chunk)} != {chunk_size}")
            chunk_sum = hashlib.md5(chunk).hexdigest()
            chunk_sums.append(chunk_sum)
    return chunk_sums


def check_chunk_integrity(src_chunk_sums, out_chunk_sums, expected_frame_percentage: int = 80) -> bool:
    logging.debug("SOURCE CHUNKS:")
    logging.debug(src_chunk_sums)
    logging.debug("OUTPUT CHUNKS:")
    logging.debug(out_chunk_sums)

    min_len = min(len(src_chunk_sums), len(out_chunk_sums))
    for i in range(min_len):
        if src_chunk_sums[i] != out_chunk_sums[i]:
            logging.debug(f"Received frame {i+1} is invalid")
            return False

    # Ensure enough frames were sent
    if len(out_chunk_sums) / len(src_chunk_sums) * 100.00 < expected_frame_percentage:
        logging.warning(f"Received only {len(out_chunk_sums)/len(src_chunk_sums) * 100:.2f}% of the frames! Expected: {expected_frame_percentage:.2f}%")
    else:
        logging.debug(f"Received {len(out_chunk_sums)/len(src_chunk_sums) * 100:.2f}% of the frames")

    return True


def check_st20p_integrity(src_url: str, out_url: str, frame_size: int, expected_frame_percentage: int = 80) -> bool:
    src_chunk_sums = calculate_chunk_hashes(src_url, frame_size)
    out_chunk_sums = calculate_chunk_hashes(out_url, frame_size)
    return check_chunk_integrity(src_chunk_sums, out_chunk_sums, expected_frame_percentage)


def calculate_yuv_frame_size(width: int, height: int, file_format: str) -> int:
    match file_format:
        case "YUV422RFC4175PG2BE10" | "yuv422p10rfc4175":
            pixel_size = 2.5
        case "YUV422PLANAR10LE" | "yuv422p10le":
            pixel_size = 4
        case _:
            log_fail(f"Size of {file_format} pixel is not known")

    return int(width * height * pixel_size)


def check_st30p_integrity(src_url: str, out_url: str, size: int) -> bool:
    src_chunk_sums = calculate_chunk_hashes(src_url, size)
    out_chunk_sums = calculate_chunk_hashes(out_url, size)
    return check_chunk_integrity(src_chunk_sums, out_chunk_sums)


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
        case "4":
            packet_time = 1_000_000 * 4

    desired_frame_time = 10_000_000

    packet_per_frame = 1

    if desired_frame_time > packet_time:
        packet_per_frame = floor(desired_frame_time / packet_time)

    return packet_per_frame * packet_size