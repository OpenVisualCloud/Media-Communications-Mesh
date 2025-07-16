# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import os
from enum import Enum


class PayloadType(Enum):
    VIDEO = "video"
    AUDIO = "audio"
    ANCIL = "ancillary"
    BLOB = "blob"


class Payload:
    """Class used to prepare payload part of connection.json config file"""

    payload_type = None

    def __init__(self, payload_type=PayloadType.VIDEO):
        self.payload_type = payload_type

    def to_dict(self) -> dict:
        if self.payload_type == PayloadType.VIDEO:
            return {
                "payload": {
                    "video": {
                        "width": self.width,
                        "height": self.height,
                        "fps": self.fps,
                        "pixelFormat": self.pixelFormat,
                    }
                }
            }
        elif self.payload_type == PayloadType.AUDIO:
            return {
                "payload": {
                    "audio": {
                        "channels": self.channels,
                        "sampleRate": self.sampleRate,
                        "format": self.audio_format,
                        "packetTime": self.packetTime,
                    }
                }
            }
        elif self.payload_type == PayloadType.ANCIL:
            return {"payload": {"ancillary": {}}}
        elif self.payload_type == PayloadType.BLOB:
            return {
                "payload": {"blob": {}},
                "maxPayloadSize": self.max_payload_size,
            }
        else:
            return {"payload": {"unknown": {}}}

    @staticmethod
    def get_media_file_path(media_path, file_info):
        return os.path.join(media_path, file_info["filename"])


class Video(Payload):
    def __init__(self, width=1920, height=1080, fps=60.0, pixelFormat="yuv422p10le", media_file_path=None):
        self.width = width
        self.height = height
        self.fps = fps
        self.pixelFormat = pixelFormat
        self.media_file_path = media_file_path
        self.payload_type = PayloadType.VIDEO

    @classmethod
    def from_file_info(cls, file_info, media_path=None):
        media_file_path = None
        if media_path and file_info and "filename" in file_info:
            media_file_path = Payload.get_media_file_path(media_path, file_info)
        return cls(
            width=file_info["width"],
            height=file_info["height"],
            fps=file_info["fps"],
            pixelFormat=cls.file_format_to_pixel_format(file_info["file_format"]),
            media_file_path=media_file_path,
        )

    @staticmethod
    def file_format_to_pixel_format(file_format):
        video_format_matches = {
            "YUV422PLANAR10LE": "yuv422p10le",
            "YUV422RFC4175PG2BE10": "yuv422p10rfc4175",
        }
        return video_format_matches.get(file_format, file_format)

    @staticmethod
    def get_media_file_path(media_path, file_info):
        return Payload.get_media_file_path(media_path, file_info)


class Audio(Payload):
    def __init__(self, channels=2, sampleRate=48000, audio_format="pcm_s24be", packetTime="1ms", media_file_path=None):
        self.channels = channels
        self.sampleRate = sampleRate
        self.audio_format = audio_format  # .payload.format (JSON)
        self.packetTime = packetTime
        self.media_file_path = media_file_path
        self.payload_type = PayloadType.AUDIO

    @classmethod
    def from_file_info(cls, file_info, media_path=None):
        media_file_path = None
        if media_path and file_info and "filename" in file_info:
            media_file_path = Payload.get_media_file_path(media_path, file_info)
        return cls(
            channels=file_info["channels"],
            sampleRate=file_info["sample_rate"],
            audio_format=file_info["format"],
            packetTime=file_info.get("packet_time", "1ms"),
            media_file_path=media_file_path,
        )

    @staticmethod
    def get_media_file_path(media_path, file_info):
        return Payload.get_media_file_path(media_path, file_info)


class Ancillary(Payload):
    def __init__(self):
        self.payload_type = PayloadType.ANCIL


class Blob(Payload):
    def __init__(self, max_payload_size=102400, media_file_path=None):
        self.payload_type = PayloadType.BLOB
        self.max_payload_size = max_payload_size
        self.media_file_path = media_file_path

    @classmethod
    def from_file_info(cls, file_info, media_path=None):
        media_file_path = None
        if media_path and file_info and "filename" in file_info:
            media_file_path = Payload.get_media_file_path(media_path, file_info)
        return cls(
            max_payload_size=file_info["max_payload_size"],
            media_file_path=media_file_path,
        )

    @staticmethod
    def get_media_file_path(media_path, file_info):
        return Payload.get_media_file_path(media_path, file_info)
