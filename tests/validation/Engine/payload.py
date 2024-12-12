# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

from enum import Enum

class PayloadType(Enum):
    VIDEO = "video",
    AUDIO = "audio",
    ANCIL = "ancillary",
    BLOB = "blob"


class Payload:
    """Class used to prepare payload part of connection.json config file"""
    payload_type = None
    def __init__(self, payload_type=PayloadType.VIDEO):
        self.payload_type = payload_type

    def toDict(self) -> dict:
        if self.payload_type == PayloadType.VIDEO:
            return {
                "payload": {
                    "video": {
                        "width": self.width,
                        "height": self.height,
                        "fps": self.fps,
                        "pixelFormat": self.pixelFormat
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
                        "packetTime": self.packetTime
                    }
                }
            }
        elif self.payload_type == PayloadType.ANCIL:
            return {
                "payload": {
                    "ancillary": {}
                }
            }
        elif self.payload_type == PayloadType.BLOB:
            return {
                "payload": {
                    "blob": {}
                }
            }
        else: # unknown payload type
            return {
                "payload": {
                    "unknown": {}
                }
            }

class Video(Payload):
    def __init__(
        self,
        width=1920,
        height=1080,
        fps=60.0,
        pixelFormat="yuv422p10le"
    ):
        self.width = width
        self.height = height
        self.fps = fps
        self.pixelFormat = pixelFormat
        self.payload_type = PayloadType.VIDEO

class Audio(Payload):
    def __init__(
        self,
        channels=2,
        sampleRate=48000,
        audio_format="pcm_s24be",
        packetTime="1ms"
    ):
        self.channels = channels
        self.sampleRate = sampleRate
        self.audio_format = audio_format # .payload.format (JSON)
        self.packetTime = packetTime
        self.payload_type = PayloadType.AUDIO

class Ancillary(Payload):
    def __init__(self):
        self.payload_type = PayloadType.ANCIL

class Blob(Payload):
    def __init__(self):
        self.payload_type = PayloadType.BLOB
