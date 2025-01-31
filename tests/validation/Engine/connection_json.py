# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import json

from .connection import Connection
from .payload import Payload


class ConnectionJson:
    """Class used to prepare connection.json config file"""

    def __init__(
        self,
        bufferQueueCapacity=16,
        maxPayloadSize=None,
        maxMetadataSize=8192,
        connection=Connection(),
        payload=Payload(),
    ):
        self.bufferQueueCapacity = bufferQueueCapacity
        self.maxPayloadSize = maxPayloadSize if type(maxPayloadSize) == int and maxPayloadSize >= 0 else None
        self.maxMetadataSize = maxMetadataSize
        self.connection = connection
        self.payload = payload

    def to_dict(self) -> dict:
        result = {
            "bufferQueueCapacity": self.bufferQueueCapacity,
            "maxMetadataSize": self.maxMetadataSize,
            **self.connection.to_dict(),
            **self.payload.to_dict(),
        }
        if self.maxPayloadSize != None:
            result.update({"maxPayloadSize": self.maxPayloadSize})
        return result

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), indent=4)

    def prepare_and_save_json(self, output_path:str = "connection.json") -> None:
        with open(output_path, "w") as connection_json_file:
            connection_json_file.write(self.to_json())
