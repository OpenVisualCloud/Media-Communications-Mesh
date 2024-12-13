# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

import json

from .connection import Connection
from .payload import Payload


class ConnectionJson:
    """Class used to prepare connection.json config file"""

    def __init__(
        self,
        bufferQueueCapacity=16,
        maxPayloadSize=2097152,
        maxMetadataSize=8192,
        connection=Connection(),
        payload=Payload(),
    ):
        self.bufferQueueCapacity = bufferQueueCapacity
        self.maxPayloadSize = maxPayloadSize
        self.maxMetadataSize = maxMetadataSize
        self.connection = connection
        self.payload = payload

    def to_dict(self) -> dict:
        return {
            "bufferQueueCapacity": self.bufferQueueCapacity,
            "maxPayloadSize": self.maxPayloadSize,
            "maxMetadataSize": self.maxMetadataSize,
            **self.connection.to_dict(),
            **self.payload.to_dict(),
        }

    def to_json(self):
        return json.dumps(self.to_dict(), indent=4)

    def prepare_and_save_json(self, output_path="connection.json"):
        with open(output_path, "w") as connection_json_file:
            connection_json_file.write(self.to_json())
