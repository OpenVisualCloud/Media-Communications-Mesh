# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import json
from pathlib import Path

from .rx_tx_app_connection import RxTxAppConnection
from .rx_tx_app_payload import Payload


class ConnectionJson:
    """Class used to prepare connection.json config file"""

    def __init__(
        self,
        host,
        bufferQueueCapacity: int = 16,
        maxPayloadSize: int = -1,
        maxMetadataSize: int = 8192,
        rx_tx_app_connection: RxTxAppConnection = RxTxAppConnection(),
        payload: Payload = Payload(),
    ):
        self.host = host
        self.bufferQueueCapacity = bufferQueueCapacity
        self.maxPayloadSize = maxPayloadSize if maxPayloadSize >= 0 else -1
        self.maxMetadataSize = maxMetadataSize
        self.rx_tx_app_connection = rx_tx_app_connection
        self.payload = payload

    def to_dict(self) -> dict:
        result = {
            "bufferQueueCapacity": self.bufferQueueCapacity,
            "maxMetadataSize": self.maxMetadataSize,
            **self.rx_tx_app_connection.to_dict(),
            **self.payload.to_dict(),
        }
        if self.maxPayloadSize >= 0:
            result.update({"maxPayloadSize": self.maxPayloadSize})
        return result

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), indent=4)

    def prepare_and_save_json(self, output_path: str = "connection.json") -> None:
        f = self.host.connection.path(output_path)
        json_content = self.to_json().replace('"', '\\"')
        f.write_text(json_content)

    def copy_json_to_logs(self, log_path: str) -> None:
        """Copy the connection.json file to the log path on runner."""
        source_path = self.host.connection.path("connection.json")
        dest_path = Path(log_path) / "connection.json"
        
        # Create log directory if it doesn't exist
        Path(log_path).mkdir(parents=True, exist_ok=True)
        
        # Copy the connection.json file to the log path
        with open(dest_path, "w") as dest_file:
            dest_file.write(self.to_json())
