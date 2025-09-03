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

    def copy_json_to_logs(self, log_path: str, filename="connection.json") -> None:
        """Copy the connection.json file to the log path on runner with specified filename.
        
        Places the file in the mesh-agent directory and includes rx/tx prefix based on instance name.
        """
        source_path = self.host.connection.path(filename)  # Original file
        
        # Create directory path under the host name directory
        mesh_agent_dir = Path(log_path) / "RxTx" / self.host.name
        mesh_agent_dir.mkdir(parents=True, exist_ok=True)
        
        # Just use the filename as provided by the caller
        # The create_connection_json function now provides the properly formatted filename
        new_filename = filename
            
        dest_path = mesh_agent_dir / new_filename

        # Copy the connection.json file to the mesh-agent directory
        with open(dest_path, "w") as dest_file:
            dest_file.write(self.to_json())
